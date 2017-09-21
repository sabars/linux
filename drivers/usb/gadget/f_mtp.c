/*
 * Gadget Function Driver for MTP
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/ch9.h>
#include <linux/usb/f_mtp.h>

#define MTP_COMPAT_IOCTL
#ifdef MTP_COMPAT_IOCTL
#include <linux/compat.h>
#include <linux/uaccess.h>
#endif

#define MTP_BULK_BUFFER_SIZE       16384
#define INTR_BUFFER_SIZE           28

#define ACTIONS_ADFU_CMD
#ifdef ACTIONS_ADFU_CMD
#include "set_serial_number.h"

#define MAX_SERIAL_NUMBER_LEN 64
/*please reference to include/linux/usb/f_mtp.h */
#define ACTIONS_MTP_MODIFY_SERIALNO   _IOW('M', 5, char *)
#define ACTIONS_MTP_MODIFY_SERIALNO_RESTART   _IOW('M', 6, char *)
#define ACTIONS_MTP_GET_SERIALNO   _IOR('M', 7, char *)
#define ACTIONS_MTP_SWITCH_TO_ADFU    _IOW('M', 8, char *)
#define ACTIONS_MTP_GET_MISCINFO   _IOR('M', 9, char *)
#define ACTIONS_MTP_SET_MISCINFO    _IOW('M', 10, char *)
#endif

#define MTP_FIX_DIRTY_BYTE 1

/* String IDs */
#define INTERFACE_STRING_INDEX	0

/* values for mtp_dev.state */
#define STATE_OFFLINE               0   /* initial state, disconnected */
#define STATE_READY                 1   /* ready for userspace calls */
#define STATE_BUSY                  2   /* processing userspace calls */
#define STATE_CANCELED              3   /* transaction canceled by host */
#define STATE_ERROR                 4   /* error from completion routine */

/* number of tx and rx requests to allocate */
#define TX_REQ_MAX 4
#define RX_REQ_MAX 2
#define INTR_REQ_MAX 5

/* ID for Microsoft MTP OS String */
#define MTP_OS_STRING_ID   0xEE

/* MTP class reqeusts */
#define MTP_REQ_CANCEL              0x64
#define MTP_REQ_GET_EXT_EVENT_DATA  0x65
#define MTP_REQ_RESET               0x66
#define MTP_REQ_GET_DEVICE_STATUS   0x67

/* constants for device status */
#define MTP_RESPONSE_OK             0x2001
#define MTP_RESPONSE_DEVICE_BUSY    0x2019

static const char mtp_shortname[] = "mtp_usb";

struct mtp_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
	struct usb_ep *ep_intr;

	int state;

	/* synchronize access to our device file */
	atomic_t open_excl;
	/* to enforce only one ioctl at a time */
	atomic_t ioctl_excl;
	atomic_t rd_excl;
	atomic_t wt_excl;

	struct list_head tx_idle;
	struct list_head intr_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	wait_queue_head_t intr_wq;
	struct usb_request *rx_req[RX_REQ_MAX];
	int rx_done;

	/* for processing MTP_SEND_FILE, MTP_RECEIVE_FILE and
	 * MTP_SEND_FILE_WITH_HEADER ioctls on a work queue
	 */
	struct workqueue_struct *wq;
	struct work_struct send_file_work;
	struct work_struct receive_file_work;
#if MTP_FIX_DIRTY_BYTE
	struct timer_list dirtybyte_timer;
	struct work_struct dirty_byte_work;
#endif
	struct file *xfer_file;
	loff_t xfer_file_offset;
	int64_t xfer_file_length;
	unsigned xfer_send_header;
	uint16_t xfer_command;
	uint32_t xfer_transaction_id;
	int xfer_result;
};

static struct usb_interface_descriptor mtp_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 3,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = USB_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol     = 0,
};

static struct usb_interface_descriptor ptp_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 3,
	.bInterfaceClass        = USB_CLASS_STILL_IMAGE,
	.bInterfaceSubClass     = 1,
	.bInterfaceProtocol     = 1,
};

static struct usb_endpoint_descriptor mtp_superspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor mtp_superspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor mtp_superspeed_bulk_comp_desc = {
	.bLength =              sizeof mtp_superspeed_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
};

static struct usb_endpoint_descriptor mtp_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mtp_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mtp_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mtp_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mtp_intr_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize         = __constant_cpu_to_le16(INTR_BUFFER_SIZE),
	.bInterval              = 6,
};

static struct usb_ss_ep_comp_descriptor mtp_intr_comp_desc = {
	.bLength =		sizeof mtp_intr_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 3 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
	.wBytesPerInterval =	__constant_cpu_to_le16(INTR_BUFFER_SIZE),
};

static struct usb_descriptor_header *fs_mtp_descs[] = {
	(struct usb_descriptor_header *) &mtp_interface_desc,
	(struct usb_descriptor_header *) &mtp_fullspeed_in_desc,
	(struct usb_descriptor_header *) &mtp_fullspeed_out_desc,
	(struct usb_descriptor_header *) &mtp_intr_desc,
	NULL,
};

static struct usb_descriptor_header *hs_mtp_descs[] = {
	(struct usb_descriptor_header *) &mtp_interface_desc,
	(struct usb_descriptor_header *) &mtp_highspeed_in_desc,
	(struct usb_descriptor_header *) &mtp_highspeed_out_desc,
	(struct usb_descriptor_header *) &mtp_intr_desc,
	NULL,
};

static struct usb_descriptor_header *ss_mtp_descs[] = {
	(struct usb_descriptor_header *) &mtp_interface_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_in_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_bulk_comp_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_out_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_bulk_comp_desc,
	(struct usb_descriptor_header *) &mtp_intr_desc,
	(struct usb_descriptor_header *) &mtp_intr_comp_desc,
	NULL,
};

static struct usb_descriptor_header *fs_ptp_descs[] = {
	(struct usb_descriptor_header *) &ptp_interface_desc,
	(struct usb_descriptor_header *) &mtp_fullspeed_in_desc,
	(struct usb_descriptor_header *) &mtp_fullspeed_out_desc,
	(struct usb_descriptor_header *) &mtp_intr_desc,
	NULL,
};

static struct usb_descriptor_header *hs_ptp_descs[] = {
	(struct usb_descriptor_header *) &ptp_interface_desc,
	(struct usb_descriptor_header *) &mtp_highspeed_in_desc,
	(struct usb_descriptor_header *) &mtp_highspeed_out_desc,
	(struct usb_descriptor_header *) &mtp_intr_desc,
	NULL,
};

static struct usb_descriptor_header *ss_ptp_descs[] = {
	(struct usb_descriptor_header *) &ptp_interface_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_in_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_bulk_comp_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_out_desc,
	(struct usb_descriptor_header *) &mtp_superspeed_bulk_comp_desc,
	(struct usb_descriptor_header *) &mtp_intr_desc,
	(struct usb_descriptor_header *) &mtp_intr_comp_desc,
	NULL,
};

static struct usb_string mtp_string_defs[] = {
	/* Naming interface "MTP" so libmtp will recognize us */
	[INTERFACE_STRING_INDEX].s	= "MTP",
	{  },	/* end of list */
};

static struct usb_gadget_strings mtp_string_table = {
	.language		= 0x0409,	/* en-US */
	.strings		= mtp_string_defs,
};

static struct usb_gadget_strings *mtp_strings[] = {
	&mtp_string_table,
	NULL,
};

/* Microsoft MTP OS String */
static u8 mtp_os_string[] = {
	18, /* sizeof(mtp_os_string) */
	USB_DT_STRING,
	/* Signature field: "MSFT100" */
	'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0,
	/* vendor code */
	1,
	/* padding */
	0
};

/* Microsoft Extended Configuration Descriptor Header Section */
struct mtp_ext_config_desc_header {
	__le32	dwLength;
	__u16	bcdVersion;
	__le16	wIndex;
	__u8	bCount;
	__u8	reserved[7];
};

/* Microsoft Extended Configuration Descriptor Function Section */
struct mtp_ext_config_desc_function {
	__u8	bFirstInterfaceNumber;
	__u8	bInterfaceCount;
	__u8	compatibleID[8];
	__u8	subCompatibleID[8];
	__u8	reserved[6];
};

/* MTP Extended Configuration Descriptor */
struct {
	struct mtp_ext_config_desc_header	header;
	struct mtp_ext_config_desc_function    function;
} mtp_ext_config_desc = {
	.header = {
		.dwLength = __constant_cpu_to_le32(sizeof(mtp_ext_config_desc)),
		.bcdVersion = __constant_cpu_to_le16(0x0100),
		.wIndex = __constant_cpu_to_le16(4),
		.bCount = __constant_cpu_to_le16(1),
	},
	.function = {
		.bFirstInterfaceNumber = 0,
		.bInterfaceCount = 1,
		.compatibleID = { 'M', 'T', 'P' },
	},
};

struct mtp_device_status {
	__le16	wLength;
	__le16	wCode;
};

struct mtp_data_header {
	/* length of packet, including this header */
	__le32	length;
	/* container type (2 for data packet) */
	__le16	type;
	/* MTP command code */
	__le16	command;
	/* MTP transaction ID */
	__le32	transaction_id;
};

/* temporary variable used between mtp_open() and mtp_gadget_bind() */
static struct mtp_dev *_mtp_dev;

static inline struct mtp_dev *func_to_mtp(struct usb_function *f)
{
	return container_of(f, struct mtp_dev, function);
}

static struct usb_request *mtp_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void mtp_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int mtp_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void mtp_unlock(atomic_t *excl)
{
	if (atomic_read(excl) != 0)
		atomic_dec(excl);
}

/* add a request to the tail of a list */
static void mtp_req_put(struct mtp_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request
*mtp_req_get(struct mtp_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void mtp_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct mtp_dev *dev = _mtp_dev;

	if (req->status != 0)
		dev->state = STATE_ERROR;

	mtp_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void mtp_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct mtp_dev *dev = _mtp_dev;

	dev->rx_done = 1;
	if (req->status != 0)
		dev->state = STATE_ERROR;

	wake_up(&dev->read_wq);
}

static void mtp_complete_intr(struct usb_ep *ep, struct usb_request *req)
{
	struct mtp_dev *dev = _mtp_dev;

	if (req->status != 0)
		dev->state = STATE_ERROR;

	mtp_req_put(dev, &dev->intr_idle, req);

	wake_up(&dev->intr_wq);
}

static int mtp_create_bulk_endpoints(struct mtp_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc,
				struct usb_endpoint_descriptor *intr_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for mtp ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	ep = usb_ep_autoconfig(cdev->gadget, intr_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_intr failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for mtp ep_intr got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_intr = ep;

	/* now allocate requests for our endpoints */
	for (i = 0; i < TX_REQ_MAX; i++) {
		req = mtp_request_new(dev->ep_in, MTP_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mtp_complete_in;
		mtp_req_put(dev, &dev->tx_idle, req);
	}
	for (i = 0; i < RX_REQ_MAX; i++) {
		req = mtp_request_new(dev->ep_out, MTP_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mtp_complete_out;
		dev->rx_req[i] = req;
	}
	for (i = 0; i < INTR_REQ_MAX; i++) {
		req = mtp_request_new(dev->ep_intr, INTR_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mtp_complete_intr;
		mtp_req_put(dev, &dev->intr_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "mtp_bind() could not allocate requests\n");
	return -1;
}

static ssize_t mtp_read(struct file *fp, char __user *buf,
	size_t count, loff_t *pos)
{
	struct mtp_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	ssize_t r = count;
	unsigned xfer;
	int ret = 0;

	if(!_mtp_dev){
		printk("%s %d error: _mtp_dev is null\n",__func__, __LINE__);
		return -ENODEV;
	}
	
	if (_mtp_dev!= dev){
		printk("%s %d error:_mtp_dev!= dev\n",__func__, __LINE__);
		return -ENODEV;
	}

	DBG(cdev, "mtp_read(%zu)\n", count);

	if (count > MTP_BULK_BUFFER_SIZE)
		return -EINVAL;

	mtp_lock(&dev->rd_excl);

	/* we will block until we're online */
	DBG(cdev, "mtp_read: waiting for online state\n");
	ret = wait_event_interruptible(dev->read_wq,
		dev->state != STATE_OFFLINE);
	if (ret < 0) {
		r = ret;
		goto done;
	}
	if((!_mtp_dev) || (_mtp_dev != dev)) {
		printk("_mtp_dev become null after mtp_read wakeup!\n");
		return -ENODEV;
	}
	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED) {
		/* report cancelation to userspace */
		dev->state = STATE_READY;
		spin_unlock_irq(&dev->lock);
		mtp_unlock(&dev->rd_excl);
		return -ECANCELED;
	}
	dev->state = STATE_BUSY;
	spin_unlock_irq(&dev->lock);

requeue_req:
	/* queue a request */
	req = dev->rx_req[0];
	req->length = count + dev->ep_out->maxpacket - 1;
	req->length -= req->length  % dev->ep_out->maxpacket;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_KERNEL);
	if (ret < 0) {
		r = -EIO;
		goto done;
	} else {
		DBG(cdev, "rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, ((dev->rx_done) || (dev->state == STATE_CANCELED)));

	if (dev->state == STATE_CANCELED) {
		usb_ep_dequeue(dev->ep_out, req);
		printk("%s %d\n", __func__, __LINE__);
		/* report cancelation to userspace */
		dev->state = STATE_READY;
		mtp_unlock(&dev->rd_excl);
		return -ECANCELED;
	}

	if (ret < 0) {
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if((!_mtp_dev) || (_mtp_dev != dev)) {
		printk("_mtp_dev become null after mtp_read wakeup2!\n");
		return -ENODEV;
	}
	if (dev->state == STATE_BUSY) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		DBG(cdev, "rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else
		r = -EIO;

done:
	if((!_mtp_dev) || (_mtp_dev != dev)) {
		printk("_mtp_dev become null after mtp_read wakeup!\n");
		return -ENODEV;
	}
	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED)
		r = -ECANCELED;
	else if (dev->state != STATE_OFFLINE)
		dev->state = STATE_READY;
	spin_unlock_irq(&dev->lock);

	mtp_unlock(&dev->rd_excl);

	DBG(cdev, "mtp_read returning %zd\n", r);
	return r;
}

static ssize_t mtp_write(struct file *fp, const char __user *buf,
	size_t count, loff_t *pos)
{
	struct mtp_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	ssize_t r = count;
	unsigned xfer;
	int sendZLP = 0;
	int ret;

	if(!_mtp_dev){
		printk("%s %d error: _mtp_dev is null\n",__func__, __LINE__);
		return -ENODEV;
	}
	
	if (_mtp_dev!= dev){
		printk("%s %d error:_mtp_dev!= dev\n",__func__, __LINE__);
		return -ENODEV;
	}

	DBG(cdev, "mtp_write(%zu)\n", count);

	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED) {
		/* report cancelation to userspace */
		dev->state = STATE_READY;
		spin_unlock_irq(&dev->lock);
		return -ECANCELED;
	}
	if (dev->state == STATE_OFFLINE) {
		spin_unlock_irq(&dev->lock);
		return -ENODEV;
	}
	dev->state = STATE_BUSY;
	spin_unlock_irq(&dev->lock);

	mtp_lock(&dev->wt_excl);

	/* we need to send a zero length packet to signal the end of transfer
	 * if the transfer size is aligned to a packet boundary.
	 */
	if ((count & (dev->ep_in->maxpacket - 1)) == 0)
		sendZLP = 1;

	while (count > 0 || sendZLP) {
		/* so we exit after sending ZLP */
		if (count == 0)
			sendZLP = 0;

		if (dev->state != STATE_BUSY) {
			DBG(cdev, "mtp_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = mtp_req_get(dev, &dev->tx_idle))
				|| dev->state != STATE_BUSY));
		if (!req) {
			r = ret;
			break;
		}

		if (count > MTP_BULK_BUFFER_SIZE)
			xfer = MTP_BULK_BUFFER_SIZE;
		else
			xfer = count;
		if (xfer && copy_from_user(req->buf, buf, xfer)) {
			r = -EFAULT;
			break;
		}

		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			DBG(cdev, "mtp_write: xfer error %d\n", ret);
			r = -EIO;
			break;
		}

		buf += xfer;
		count -= xfer;

		/* zero this so we don't try to free it on error exit */
		req = 0;
	}

	if((!_mtp_dev) || (_mtp_dev != dev)) {
		printk("_mtp_dev become null after mtp_write wakeup!\n");
		return -ENODEV;
	}
	if (req)
		mtp_req_put(dev, &dev->tx_idle, req);

	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED)
		r = -ECANCELED;
	else if (dev->state != STATE_OFFLINE)
		dev->state = STATE_READY;
	spin_unlock_irq(&dev->lock);

	DBG(cdev, "mtp_write returning %zd\n", r);
	mtp_unlock(&dev->wt_excl);
	return r;
}

/* read from a local file and write to USB */
static void send_file_work(struct work_struct *data)
{
	struct mtp_dev *dev = container_of(data, struct mtp_dev,
						send_file_work);
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	struct mtp_data_header *header;
	struct file *filp;
	loff_t offset;
	int64_t count;
	int xfer, ret, hdr_size;
	int r = 0;
	int sendZLP = 0;

	/* read our parameters */
	smp_rmb();
	filp = dev->xfer_file;
	offset = dev->xfer_file_offset;
	count = dev->xfer_file_length;

	DBG(cdev, "send_file_work(%lld %lld)\n", offset, count);

	if (dev->xfer_send_header) {
		hdr_size = sizeof(struct mtp_data_header);
		count += hdr_size;
	} else {
		hdr_size = 0;
	}

	/* we need to send a zero length packet to signal the end of transfer
	 * if the transfer size is aligned to a packet boundary.
	 */
	if ((count & (dev->ep_in->maxpacket - 1)) == 0)
		sendZLP = 1;

	while (count > 0 || sendZLP) {
		/* so we exit after sending ZLP */
		if (count == 0)
			sendZLP = 0;

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			(req = mtp_req_get(dev, &dev->tx_idle))
			|| dev->state != STATE_BUSY);
		if (dev->state == STATE_CANCELED) {
			r = -ECANCELED;
			break;
		}
		if (!req) {
			r = ret;
			break;
		}

		if (count > MTP_BULK_BUFFER_SIZE)
			xfer = MTP_BULK_BUFFER_SIZE;
		else
			xfer = count;

		if (hdr_size) {
			/* prepend MTP data header */
			header = (struct mtp_data_header *)req->buf;
			header->length = __cpu_to_le32(count);
			header->type = __cpu_to_le16(2); /* data packet */
			header->command = __cpu_to_le16(dev->xfer_command);
			header->transaction_id =
					__cpu_to_le32(dev->xfer_transaction_id);
		}

		ret = vfs_read(filp, req->buf + hdr_size, xfer - hdr_size,
								&offset);
		if (ret < 0) {
			r = ret;
			break;
		}
		xfer = ret + hdr_size;
		hdr_size = 0;

		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			DBG(cdev, "send_file_work: xfer error %d\n", ret);
			dev->state = STATE_ERROR;
			r = -EIO;
			break;
		}

		count -= xfer;

		/* zero this so we don't try to free it on error exit */
		req = 0;
	}

	if (req)
		mtp_req_put(dev, &dev->tx_idle, req);

	DBG(cdev, "send_file_work returning %d\n", r);
	/* write the result */
	dev->xfer_result = r;
	smp_wmb();
}

#if MTP_FIX_DIRTY_BYTE
#define  PROC_DIRTY_BYTES_PATH      "/proc/sys/vm/dirty_bytes"
#define  PROC_DIRTY_RATIO_PATH      "/proc/sys/vm/dirty_ratio"
#define  PROC_DIRTY_BYTES_VAL	    "8388608"
#define  PROC_DIRTY_RATIO_VAL	    "20"
#define DIRTY_BYTES_WRITE_DISABLE 0
#define DIRTY_BYTES_WRITE_ENABLE 1
#define RECEIVE_FILE_STATUS_IDLE 0
#define RECEIVE_FILE_STATUS_BUSY 1
static int write_dirtybytes_flag = DIRTY_BYTES_WRITE_DISABLE;
static int receive_file_status = RECEIVE_FILE_STATUS_IDLE;
static unsigned int file_cnt=0;

static struct file *dirty_ratio_filp = NULL;
char dirty_ratio_val[32];
#define DELAY_TIME  10000 //wait 10s to cancel dirty byte

static void dirtybyte_write_timer_func(unsigned long h)
{
	static unsigned int old_file_cnt=0;

	if(write_dirtybytes_flag == DIRTY_BYTES_WRITE_ENABLE){
		if((file_cnt != old_file_cnt)||(receive_file_status ==RECEIVE_FILE_STATUS_BUSY)) {
				old_file_cnt = file_cnt;
				mod_timer(&_mtp_dev->dirtybyte_timer, jiffies+msecs_to_jiffies(DELAY_TIME));
		}
		else if(receive_file_status == RECEIVE_FILE_STATUS_IDLE){
			queue_work(_mtp_dev->wq, &_mtp_dev->dirty_byte_work);
		}
	}
}

static void dirty_byte_work(struct work_struct *data)
{
	mm_segment_t old_fs;
	loff_t file_offset =0;
	int dirty_write_cnt;

	write_dirtybytes_flag =DIRTY_BYTES_WRITE_DISABLE;
	del_timer_sync(&_mtp_dev->dirtybyte_timer);
	if (dirty_ratio_filp == NULL)
		goto out;

	dirty_ratio_filp = filp_open(PROC_DIRTY_RATIO_PATH, O_RDWR, 0);
	if (IS_ERR(dirty_ratio_filp)) {
		printk("open %s error!\n", PROC_DIRTY_RATIO_PATH);
		goto out;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	file_offset = 0;
	dirty_write_cnt = vfs_write(dirty_ratio_filp, dirty_ratio_val, strlen(dirty_ratio_val), &file_offset);
	printk("write %s: %d\n", dirty_ratio_val, dirty_write_cnt);
	set_fs(old_fs);

	filp_close(dirty_ratio_filp, NULL);

out:
	dirty_ratio_filp = NULL;
	return;
}

static void write_dirty_byte_file(void)
{
	struct file *dirty_bytes_filp = NULL;
	mm_segment_t old_fs;
	loff_t file_offset =0;
	int dirty_bytes_length;
	int dirty_write_cnt;
	char dirty_tmp_buf[32];


	dirty_bytes_filp = filp_open(PROC_DIRTY_BYTES_PATH, O_RDWR, 0);
	if (IS_ERR(dirty_bytes_filp)) {
		printk("open %s error!\n", PROC_DIRTY_BYTES_PATH);
		goto err;
	}

	dirty_ratio_filp = filp_open(PROC_DIRTY_RATIO_PATH, O_RDWR, 0);
	if (IS_ERR(dirty_ratio_filp)) {
		filp_close(dirty_bytes_filp, NULL);
		printk("open %s error!\n", PROC_DIRTY_RATIO_PATH);
		goto err;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	dirty_bytes_length = vfs_read(dirty_ratio_filp, dirty_ratio_val, 31, &file_offset);
	dirty_ratio_val[31] = '\0';
	printk("read %s: %d\n", dirty_ratio_val, dirty_bytes_length);

	file_offset = 0;
	dirty_bytes_length = strlen(PROC_DIRTY_BYTES_VAL);
	memcpy(dirty_tmp_buf, PROC_DIRTY_BYTES_VAL, dirty_bytes_length);
	dirty_tmp_buf[dirty_bytes_length] = '\0';
	dirty_write_cnt = vfs_write(dirty_bytes_filp, dirty_tmp_buf,dirty_bytes_length, &file_offset);
	printk("write %s: %d\n", dirty_tmp_buf, dirty_write_cnt);


	set_fs(old_fs);
	filp_close(dirty_bytes_filp, NULL);
	filp_close(dirty_ratio_filp, NULL);
	_mtp_dev->dirtybyte_timer.expires = jiffies +msecs_to_jiffies(DELAY_TIME);
	add_timer(&_mtp_dev->dirtybyte_timer);
	write_dirtybytes_flag =DIRTY_BYTES_WRITE_ENABLE;
	goto out;
err:
	dirty_bytes_filp = NULL;
	dirty_ratio_filp = NULL;
out:
	return;
}
#endif
/* read from USB and write to a local file */
static void receive_file_work(struct work_struct *data)
{
	struct mtp_dev *dev = container_of(data, struct mtp_dev,
						receive_file_work);
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *read_req = NULL, *write_req = NULL;
	struct file *filp;
	loff_t offset;
	int64_t count;
	int ret, cur_buf = 0;
	int r = 0;

#if MTP_FIX_DIRTY_BYTE
	if(write_dirtybytes_flag ==DIRTY_BYTES_WRITE_DISABLE)
		write_dirty_byte_file();
	
	receive_file_status =RECEIVE_FILE_STATUS_BUSY;
#endif
	/* read our parameters */
	smp_rmb();
	filp = dev->xfer_file;
	offset = dev->xfer_file_offset;
	count = dev->xfer_file_length;

	DBG(cdev, "receive_file_work(%lld)\n", count);

	while (count > 0 || write_req) {
		if (count > 0) {
			/* queue a request */
			read_req = dev->rx_req[cur_buf];
			cur_buf = (cur_buf + 1) % RX_REQ_MAX;

			read_req->length = (count > MTP_BULK_BUFFER_SIZE
					? MTP_BULK_BUFFER_SIZE : count);

			read_req->length  += dev->ep_out->maxpacket - 1;
			read_req->length  -= read_req->length  % dev->ep_out->maxpacket;
			dev->rx_done = 0;
			ret = usb_ep_queue(dev->ep_out, read_req, GFP_KERNEL);
			if (ret < 0) {
				r = -EIO;
				dev->state = STATE_ERROR;
				break;
			}
		}

		if (write_req) {
			DBG(cdev, "rx %p %d\n", write_req, write_req->actual);
			ret = vfs_write(filp, write_req->buf, write_req->actual,
				&offset);
			DBG(cdev, "vfs_write %d\n", ret);
			if (ret != write_req->actual) {
				r = -EIO;
				dev->state = STATE_ERROR;
				break;
			}
			write_req = NULL;
		}

		if (read_req) {
			/* wait for our last read to complete */
			ret = wait_event_interruptible(dev->read_wq,
				dev->rx_done || dev->state != STATE_BUSY);
			if (dev->state == STATE_CANCELED) {
				r = -ECANCELED;
				if (!dev->rx_done)
					usb_ep_dequeue(dev->ep_out, read_req);
				break;
			}
			/* if xfer_file_length is 0xFFFFFFFF, then we read until
			 * we get a zero length packet
			 */
			if (count != 0xFFFFFFFF)
				count -= read_req->actual;
			if (read_req->actual < read_req->length) {
				/*
				 * short packet is used to signal EOF for
				 * sizes > 4 gig
				 */
				DBG(cdev, "got short packet\n");
				count = 0;
			}

			write_req = read_req;
			read_req = NULL;
		}
	}

	DBG(cdev, "receive_file_work returning %d\n", r);
	/* write the result */
	dev->xfer_result = r;
	smp_wmb();
#if MTP_FIX_DIRTY_BYTE
	file_cnt++;
	receive_file_status =RECEIVE_FILE_STATUS_IDLE;
#endif
}

static int mtp_send_event(struct mtp_dev *dev, struct mtp_event *event)
{
	struct usb_request *req = NULL;
	int ret;
	int length = event->length;

	DBG(dev->cdev, "mtp_send_event(%zu)\n", event->length);

	if (length < 0 || length > INTR_BUFFER_SIZE)
		return -EINVAL;
	if (dev->state == STATE_OFFLINE)
		return -ENODEV;

	ret = wait_event_interruptible_timeout(dev->intr_wq,
			(req = mtp_req_get(dev, &dev->intr_idle)),
			msecs_to_jiffies(1000));
	if (!req)
		return -ETIME;

	if (copy_from_user(req->buf, (void __user *)event->data, length)) {
		mtp_req_put(dev, &dev->intr_idle, req);
		return -EFAULT;
	}
	req->length = length;
	ret = usb_ep_queue(dev->ep_intr, req, GFP_KERNEL);
	if (ret)
		mtp_req_put(dev, &dev->intr_idle, req);

	return ret;
}

static long mtp_ioctl(struct file *fp, unsigned code, unsigned long value)
{
	struct mtp_dev *dev = fp->private_data;
	struct file *filp = NULL;
	int ret = -EINVAL;

	if (mtp_lock(&dev->ioctl_excl))
		return -EBUSY;

	switch (code) {
	case MTP_SEND_FILE:
	case MTP_RECEIVE_FILE:
	case MTP_SEND_FILE_WITH_HEADER:
	{
		struct mtp_file_range	mfr;
		struct work_struct *work;

		spin_lock_irq(&dev->lock);
		if (dev->state == STATE_CANCELED) {
			/* report cancelation to userspace */
			dev->state = STATE_READY;
			spin_unlock_irq(&dev->lock);
			ret = -ECANCELED;
			goto out;
		}
		if (dev->state == STATE_OFFLINE) {
			spin_unlock_irq(&dev->lock);
			ret = -ENODEV;
			goto out;
		}
		dev->state = STATE_BUSY;
		spin_unlock_irq(&dev->lock);

		if (copy_from_user(&mfr, (void __user *)value, sizeof(mfr))) {
			ret = -EFAULT;
			goto fail;
		}
		/* hold a reference to the file while we are working with it */
		filp = fget(mfr.fd);
		if (!filp) {
			ret = -EBADF;
			goto fail;
		}

		/* write the parameters */
		dev->xfer_file = filp;
		dev->xfer_file_offset = mfr.offset;
		dev->xfer_file_length = mfr.length;
		smp_wmb();

		if (code == MTP_SEND_FILE_WITH_HEADER) {
			work = &dev->send_file_work;
			dev->xfer_send_header = 1;
			dev->xfer_command = mfr.command;
			dev->xfer_transaction_id = mfr.transaction_id;
		} else if (code == MTP_SEND_FILE) {
			work = &dev->send_file_work;
			dev->xfer_send_header = 0;
		} else {
			work = &dev->receive_file_work;
		}

		/* We do the file transfer on a work queue so it will run
		 * in kernel context, which is necessary for vfs_read and
		 * vfs_write to use our buffers in the kernel address space.
		 */
		queue_work(dev->wq, work);
		/* wait for operation to complete */
		flush_workqueue(dev->wq);
		fput(filp);

		/* read the result */
		smp_rmb();
		ret = dev->xfer_result;
		break;
	}
	case MTP_SEND_EVENT:
	{
		struct mtp_event	event;
		/* return here so we don't change dev->state below,
		 * which would interfere with bulk transfer state.
		 */
		if (copy_from_user(&event, (void __user *)value, sizeof(event)))
			ret = -EFAULT;
		else
			ret = mtp_send_event(dev, &event);
		goto out;
	}
#ifdef ACTIONS_ADFU_CMD
	case ACTIONS_MTP_MODIFY_SERIALNO:
	{
		char serial_buf[MAX_SERIAL_NUMBER_LEN];

		if (copy_from_user(serial_buf, (void __user *)value, MAX_SERIAL_NUMBER_LEN)) {
			ret = -EFAULT;
			goto fail;
		}
		else {
			ret = set_serial_number((char *)&serial_buf[1], serial_buf[0]);
		}
		
		goto out;
	}
	case ACTIONS_MTP_MODIFY_SERIALNO_RESTART:
	{
		gadget_andorid_shutdown_machine();
		goto out;
	}
	case ACTIONS_MTP_GET_SERIALNO:
	{
		char serial_buf[MAX_SERIAL_NUMBER_LEN];
		int len = get_serial_number_len();

		ret = get_serial_number((char *)&serial_buf[1], len);
		if(ret){
			printk("<%s %d>.get_serial_number error\n",__func__,__LINE__);
			goto out;
		}
		
		serial_buf[0] = (char)len;
		if (copy_to_user((void __user *)value, serial_buf, MAX_SERIAL_NUMBER_LEN)){
			ret = -EFAULT;
			printk("<%s %d>.copy_to_user error\n",__func__,__LINE__);
			goto fail;
		}
		
		goto out;
	}
	case ACTIONS_MTP_SET_MISCINFO:
	{
		char *miscinfo;

		miscinfo = kmalloc(MAX_MISCINFO_LEN, GFP_KERNEL);
		if(!miscinfo) {
			printk("%s %d kmalloc buf for misc info fail\n", __FUNCTION__, __LINE__);
			ret = -EFAULT;
			goto fail;
		}

		memset(miscinfo, 0, MAX_MISCINFO_LEN);
		if (copy_from_user(miscinfo, (void __user *)value, MAX_MISCINFO_LEN)){
			kfree(miscinfo);
			ret = -EFAULT;
			goto fail;
		}
		else {
			ret = set_miscinfo(miscinfo, MAX_MISCINFO_LEN);
		}
		
		kfree(miscinfo);
		goto out;
	}
	case ACTIONS_MTP_GET_MISCINFO:
	{
		char *miscinfo;
		int len;

		miscinfo = kmalloc(MAX_MISCINFO_LEN, GFP_KERNEL);
		if(!miscinfo) {
			printk("%s %d kmalloc buf for misc info fail\n", __FUNCTION__, __LINE__);
			ret = -EFAULT;
			goto fail;
		}

		memset(miscinfo, 0, MAX_MISCINFO_LEN);
		len = get_miscinfo(miscinfo);
		if(-1 == len) {
			kfree(miscinfo);
			printk("<%s %d>.get_miscinfo error\n",__func__,__LINE__);
			goto out;
		}
		
		ret = 0;
		if (copy_to_user((void __user *)value, miscinfo, MAX_MISCINFO_LEN)){
			kfree(miscinfo);
			ret = -EFAULT;
			printk("<%s %d>.copy_to_user error\n",__func__,__LINE__);
			goto fail;
		}
		
		kfree(miscinfo);
		goto out;
	}
	case ACTIONS_MTP_SWITCH_TO_ADFU:
	{
		mdelay(100);
		usb_gadget_disconnect(dev->cdev->gadget);
		set_upgrade_flags_and_restart();
		goto out;
	}
#endif
	}

fail:
	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED)
		ret = -ECANCELED;
	else if (dev->state != STATE_OFFLINE)
		dev->state = STATE_READY;
	spin_unlock_irq(&dev->lock);
out:
	mtp_unlock(&dev->ioctl_excl);
	DBG(dev->cdev, "ioctl returning %d\n", ret);
	return ret;
}

#ifdef MTP_COMPAT_IOCTL
struct mtp_event_32 {
	/* size of the event */
	u32		length;
	/* event data to send */
	u32		data;
};

struct mtp_event_64 {
	/* size of the event */
	size_t		length;
	/* event data to send */
	void __user *data;
};

static long mtp_compat_ioctl(struct file *fp, unsigned code, unsigned long value32)
{
	unsigned long value64 = value32;
	int ret = -EINVAL;

	if ((_IOC_NR(code) >= 5 && _IOC_NR(code) <= 10)
				&& _IOC_SIZE(code) == sizeof(compat_uptr_t)) {
		code &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
		code |= sizeof(char *) << _IOC_SIZESHIFT;
	}

	if (_IOC_NR(code) == 3
			&& _IOC_SIZE(code) == sizeof(struct mtp_event_32)) {
		code &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
		code |= sizeof(struct mtp_event) << _IOC_SIZESHIFT;
	}

	switch (code) {
	case MTP_SEND_EVENT:
		{
			u32	length;
			u32  datap;

			struct mtp_event_32  __user  *event32;
			struct mtp_event_64  __user  *event64;

			event32 = (struct mtp_event_32  __user  *)value32;
			event64 = compat_alloc_user_space(sizeof(*event64));
			if (event64 == NULL)
				return -ENOMEM;

			if (get_user(length, &event32->length) || put_user((size_t)length, &event64->length))
				return -EFAULT;

			if (get_user(datap, &event32->data) || put_user((void __user *)(unsigned long)datap, &event64->data))
				return -EFAULT;

			value64 = (unsigned long)event64;

			break;
		}

	default:
		break;
	}

	ret = mtp_ioctl(fp, code, value64);

	return ret;
}
#endif

static int mtp_open(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "mtp_open\n");
	if (mtp_lock(&_mtp_dev->open_excl))
		return -EBUSY;

	/* clear any error condition */
	if (_mtp_dev->state != STATE_OFFLINE)
		_mtp_dev->state = STATE_READY;

	fp->private_data = _mtp_dev;
	return 0;
}

static int mtp_release(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "mtp_release\n");

	mtp_unlock(&_mtp_dev->open_excl);
	return 0;
}

/* file operations for /dev/mtp_usb */
static const struct file_operations mtp_fops = {
	.owner = THIS_MODULE,
	.read = mtp_read,
	.write = mtp_write,
	.unlocked_ioctl = mtp_ioctl,
#ifdef MTP_COMPAT_IOCTL
	.compat_ioctl = mtp_compat_ioctl,
#endif
	.open = mtp_open,
	.release = mtp_release,
};

static struct miscdevice mtp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = mtp_shortname,
	.fops = &mtp_fops,
};

static int mtp_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	struct mtp_dev *dev = _mtp_dev;
	int	value = -EOPNOTSUPP;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);
	unsigned long	flags;

	VDBG(cdev, "mtp_ctrlrequest "
			"%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

	/* Handle MTP OS string */
	if (ctrl->bRequestType ==
			(USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE)
			&& ctrl->bRequest == USB_REQ_GET_DESCRIPTOR
			&& (w_value >> 8) == USB_DT_STRING
			&& (w_value & 0xFF) == MTP_OS_STRING_ID) {
		value = (w_length < sizeof(mtp_os_string)
				? w_length : sizeof(mtp_os_string));
		memcpy(cdev->req->buf, mtp_os_string, value);
	} else if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		/* Handle MTP OS descriptor */
		DBG(cdev, "vendor request: %d index: %d value: %d length: %d\n",
			ctrl->bRequest, w_index, w_value, w_length);

		if (ctrl->bRequest == 1
				&& (ctrl->bRequestType & USB_DIR_IN)
				&& (w_index == 4 || w_index == 5)) {
			value = (w_length < sizeof(mtp_ext_config_desc) ?
					w_length : sizeof(mtp_ext_config_desc));
			memcpy(cdev->req->buf, &mtp_ext_config_desc, value);
		}
	} else if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		DBG(cdev, "class request: %d index: %d value: %d length: %d\n",
			ctrl->bRequest, w_index, w_value, w_length);

		if (ctrl->bRequest == MTP_REQ_CANCEL && w_index == 0
				&& w_value == 0) {
			DBG(cdev, "MTP_REQ_CANCEL\n");

			spin_lock_irqsave(&dev->lock, flags);
			if (dev->state == STATE_BUSY) {
				dev->state = STATE_CANCELED;
				wake_up(&dev->read_wq);
				wake_up(&dev->write_wq);
			}
			spin_unlock_irqrestore(&dev->lock, flags);

			/* We need to queue a request to read the remaining
			 *  bytes, but we don't actually need to look at
			 * the contents.
			 */
			value = w_length;
		} else if (ctrl->bRequest == MTP_REQ_GET_DEVICE_STATUS
				&& w_index == 0 && w_value == 0) {
			struct mtp_device_status *status = cdev->req->buf;
			status->wLength =
				__constant_cpu_to_le16(sizeof(*status));

			DBG(cdev, "MTP_REQ_GET_DEVICE_STATUS\n");
			spin_lock_irqsave(&dev->lock, flags);
			/* device status is "busy" until we report
			 * the cancelation to userspace
			 */
			if (dev->state == STATE_CANCELED)
				status->wCode =
					__cpu_to_le16(MTP_RESPONSE_DEVICE_BUSY);
			else
				status->wCode =
					__cpu_to_le16(MTP_RESPONSE_OK);
			spin_unlock_irqrestore(&dev->lock, flags);
			value = sizeof(*status);
		}
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		int rc;
		cdev->req->zero = value < w_length;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0)
			ERROR(cdev, "%s: response queue error\n", __func__);
	}
	return value;
}

static int
mtp_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct mtp_dev	*dev = func_to_mtp(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "mtp_function_bind dev: %p\n", dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	mtp_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = mtp_create_bulk_endpoints(dev, &mtp_fullspeed_in_desc,
			&mtp_fullspeed_out_desc, &mtp_intr_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		mtp_highspeed_in_desc.bEndpointAddress =
			mtp_fullspeed_in_desc.bEndpointAddress;
		mtp_highspeed_out_desc.bEndpointAddress =
			mtp_fullspeed_out_desc.bEndpointAddress;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		mtp_superspeed_in_desc.bEndpointAddress =
			mtp_fullspeed_in_desc.bEndpointAddress;
		mtp_superspeed_out_desc.bEndpointAddress =
			mtp_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
mtp_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct mtp_dev	*dev = func_to_mtp(f);
	struct usb_request *req;
	int i;

	while ((req = mtp_req_get(dev, &dev->tx_idle)))
		mtp_request_free(req, dev->ep_in);
	for (i = 0; i < RX_REQ_MAX; i++)
		mtp_request_free(dev->rx_req[i], dev->ep_out);
	while ((req = mtp_req_get(dev, &dev->intr_idle)))
		mtp_request_free(req, dev->ep_intr);
	dev->state = STATE_OFFLINE;
}

static int mtp_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct mtp_dev	*dev = func_to_mtp(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "mtp_function_set_alt intf: %d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_intr);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_intr);
	if (ret) {
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	dev->state = STATE_READY;

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void mtp_function_disable(struct usb_function *f)
{
	struct mtp_dev	*dev = func_to_mtp(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	DBG(cdev, "mtp_function_disable\n");
	dev->state = STATE_OFFLINE;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);
	usb_ep_disable(dev->ep_intr);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int mtp_bind_config(struct usb_configuration *c, bool ptp_config)
{
	struct mtp_dev *dev = _mtp_dev;
	int ret = 0;

	printk(KERN_INFO "mtp_bind_config\n");

	/* allocate a string ID for our interface */
	if (mtp_string_defs[INTERFACE_STRING_INDEX].id == 0) {
		ret = usb_string_id(c->cdev);
		if (ret < 0)
			return ret;
		mtp_string_defs[INTERFACE_STRING_INDEX].id = ret;
		mtp_interface_desc.iInterface = ret;
	}

	dev->cdev = c->cdev;
	dev->function.name = "mtp";
	dev->function.strings = mtp_strings;
	if (ptp_config) {
		dev->function.fs_descriptors = fs_ptp_descs;
		dev->function.hs_descriptors = hs_ptp_descs;
		dev->function.ss_descriptors = ss_ptp_descs;
	} else {
		dev->function.fs_descriptors = fs_mtp_descs;
		dev->function.hs_descriptors = hs_mtp_descs;
		dev->function.ss_descriptors = ss_mtp_descs;
	}
	dev->function.bind = mtp_function_bind;
	dev->function.unbind = mtp_function_unbind;
	dev->function.set_alt = mtp_function_set_alt;
	dev->function.disable = mtp_function_disable;

	return usb_add_function(c, &dev->function);
}

static int mtp_setup(void)
{
	struct mtp_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);
	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	init_waitqueue_head(&dev->intr_wq);
	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->ioctl_excl, 0);
	atomic_set(&dev->rd_excl, 0);
	atomic_set(&dev->wt_excl, 0);
	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->intr_idle);

	dev->wq = create_singlethread_workqueue("f_mtp");
	if (!dev->wq) {
		ret = -ENOMEM;
		goto err1;
	}
	INIT_WORK(&dev->send_file_work, send_file_work);
	INIT_WORK(&dev->receive_file_work, receive_file_work);

#if MTP_FIX_DIRTY_BYTE
	INIT_WORK(&dev->dirty_byte_work, dirty_byte_work);
	setup_timer(&dev->dirtybyte_timer, dirtybyte_write_timer_func, 0);
#endif

	_mtp_dev = dev;

	ret = misc_register(&mtp_device);
	if (ret)
		goto err2;

	return 0;

err2:
	destroy_workqueue(dev->wq);
err1:
	_mtp_dev = NULL;
	kfree(dev);
	printk(KERN_ERR "mtp gadget driver failed to initialize\n");
	return ret;
}

static void mtp_cleanup(void)
{
	struct mtp_dev *dev = _mtp_dev;
	int cnt = 1000;

	if (!dev)
		return;

	while ((atomic_read(&dev->rd_excl) != 0)
	    || (atomic_read(&dev->wt_excl) != 0)
	    || (atomic_read(&dev->open_excl) != 0)
	    || (atomic_read(&dev->ioctl_excl) != 0)) {
		msleep(1);
		cnt--;
		if (cnt < 0) {
			printk("wait mtp read write timeout!\n");
			break;
		}
	}
	misc_deregister(&mtp_device);
	destroy_workqueue(dev->wq);

#if MTP_FIX_DIRTY_BYTE
	if(write_dirtybytes_flag ==DIRTY_BYTES_WRITE_ENABLE){
		dirty_byte_work(NULL);
	}
	del_timer_sync(&_mtp_dev->dirtybyte_timer);
#endif
	_mtp_dev = NULL;
	kfree(dev);
}
