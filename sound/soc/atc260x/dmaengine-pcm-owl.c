/*
 *  Copyright (C) 2012, Analog Devices Inc.
 *	Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 *  Based on:
 *	imx-pcm-dma-mx2.c, Copyright 2009 Sascha Hauer <s.hauer@pengutronix.de>
 *	mxs-pcm.c, Copyright (C) 2011 Freescale Semiconductor, Inc.
 *	ep93xx-pcm.c, Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 *		      Copyright (C) 2006 Applied Data Systems
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <sound/dmaengine_pcm.h>
#include "sndrv-owl.h"


extern struct dma_chan *dai_dma_slave_tx_chan(void);
extern struct dma_chan *dai_dma_slave_rx_chan(void);
extern struct dma_chan *dai_dma_slave_hdmia_chan(void);
extern struct dma_chan* dai_dma_slave_pcm1tx_chan(void);
extern struct dma_chan* dai_dma_slave_pcm1rx_chan(void);


struct dmaengine_pcm_runtime_data {
	struct dma_chan *dma_chan;
	dma_cookie_t cookie;

	unsigned int pos;
	unsigned int startflag;
};

static inline struct dmaengine_pcm_runtime_data *substream_to_prtd(
	const struct snd_pcm_substream *substream)
{
	return substream->runtime->private_data;
}

struct dma_chan *snd_dmaengine_pcm_get_chan(struct snd_pcm_substream *substream)
{
	struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);

	return prtd->dma_chan;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_get_chan);

/**
 * snd_hwparams_to_dma_slave_config - Convert hw_params to dma_slave_config
 * @substream: PCM substream
 * @params: hw_params
 * @slave_config: DMA slave config
 *
 * This function can be used to initialize a dma_slave_config from a substream
 * and hw_params in a dmaengine based PCM driver implementation.
 */
int snd_hwparams_to_dma_slave_config(const struct snd_pcm_substream *substream,
	const struct snd_pcm_hw_params *params,
	struct dma_slave_config *slave_config)
{
	enum dma_slave_buswidth buswidth;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config->direction = DMA_MEM_TO_DEV;
		slave_config->dst_addr_width = buswidth;
	} else {
		slave_config->direction = DMA_DEV_TO_MEM;
		slave_config->src_addr_width = buswidth;
	}

	slave_config->device_fc = false;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hwparams_to_dma_slave_config);

/**
 * snd_dmaengine_pcm_set_config_from_dai_data() - Initializes a dma slave config
 *  using DAI DMA data.
 * @substream: PCM substream
 * @dma_data: DAI DMA data
 * @slave_config: DMA slave configuration
 *
 * Initializes the {dst,src}_addr, {dst,src}_maxburst, {dst,src}_addr_width and
 * slave_id fields of the DMA slave config from the same fields of the DAI DMA
 * data struct. The src and dst fields will be initialized depending on the
 * direction of the substream. If the substream is a playback stream the dst
 * fields will be initialized, if it is a capture stream the src fields will be
 * initialized. The {dst,src}_addr_width field will only be initialized if the
 * addr_width field of the DAI DMA data struct is not equal to
 * DMA_SLAVE_BUSWIDTH_UNDEFINED.
 */
void snd_dmaengine_pcm_set_config_from_dai_data(
	const struct snd_pcm_substream *substream,
	const struct snd_dmaengine_dai_dma_data *dma_data,
	struct dma_slave_config *slave_config)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config->dst_addr = dma_data->addr;
		slave_config->dst_maxburst = dma_data->maxburst;
		if (dma_data->addr_width != DMA_SLAVE_BUSWIDTH_UNDEFINED)
			slave_config->dst_addr_width = dma_data->addr_width;
	} else {
		slave_config->src_addr = dma_data->addr;
		slave_config->src_maxburst = dma_data->maxburst;
		if (dma_data->addr_width != DMA_SLAVE_BUSWIDTH_UNDEFINED)
			slave_config->src_addr_width = dma_data->addr_width;
	}

	slave_config->slave_id = dma_data->slave_id;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_set_config_from_dai_data);

static void dmaengine_pcm_dma_complete(void *arg)
{
	struct snd_pcm_substream *substream = arg;
	/*struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);*/
	struct dmaengine_pcm_runtime_data *prtd = NULL;

	if (PCM_RUNTIME_CHECK(substream))
		return;

    prtd = substream_to_prtd(substream);
	if (prtd->startflag == 1) {
		if (SNDRV_PCM_STREAM_CAPTURE == substream->stream) {
			prtd->pos += snd_pcm_lib_period_bytes(substream);
			if (prtd->pos >= snd_pcm_lib_buffer_bytes(substream))
				prtd->pos = 0;
		} else if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream) {
			int prev_pos = prtd->pos;
			int diff;
			int this_interrupt_num;

			struct dma_tx_state state;
			enum dma_status status;

			int sum_periods = (snd_pcm_lib_buffer_bytes(substream)
								/snd_pcm_lib_period_bytes(substream));

			int remain_frame_cnt;
			status = dmaengine_tx_status(prtd->dma_chan, prtd->cookie, &state);
			remain_frame_cnt = state.residue/snd_pcm_lib_period_bytes(substream);

			if (state.residue%snd_pcm_lib_period_bytes(substream)) {
				remain_frame_cnt++;
			}

			remain_frame_cnt = remain_frame_cnt%sum_periods;
			prtd->pos = snd_pcm_lib_buffer_bytes(substream)
						- (remain_frame_cnt*snd_pcm_lib_period_bytes(substream));
			if (prtd->pos >= snd_pcm_lib_buffer_bytes(substream))
				prtd->pos = 0;

			diff = (prtd->pos - prev_pos);
			if (diff > 0) {
				this_interrupt_num = diff/snd_pcm_lib_period_bytes(substream);
				if ((this_interrupt_num != 1))
					printk(KERN_ERR"[audio]:[%d]lost IRQ:num = %d\n",
							__LINE__, this_interrupt_num);
			} else {
				this_interrupt_num = (diff + snd_pcm_lib_buffer_bytes(substream))
					/snd_pcm_lib_period_bytes(substream);
				if ((this_interrupt_num != 1))
					printk(KERN_ERR"[audio]:[%d]lost IRQ:num = %d\n",
							__LINE__, this_interrupt_num);
			}
		}

		snd_pcm_period_elapsed(substream);
	} else {

		printk(KERN_ERR"[audio]stream mode error\n");
	}
}

static int dmaengine_pcm_prepare_and_submit(struct snd_pcm_substream *substream)
{
	struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct dma_chan *chan = prtd->dma_chan;
	struct dma_async_tx_descriptor *desc;
	enum dma_transfer_direction direction;
	unsigned long flags = DMA_CTRL_ACK;

	direction = snd_pcm_substream_to_dma_direction(substream);

	if (!substream->runtime->no_period_wakeup)
		flags |= DMA_PREP_INTERRUPT;

	prtd->pos = 0;
	desc = dmaengine_prep_dma_cyclic(chan,
		substream->runtime->dma_addr,
		snd_pcm_lib_buffer_bytes(substream),
		snd_pcm_lib_period_bytes(substream), direction, flags);

	if (!desc) {
		printk("%s %d\n", __FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	desc->callback = dmaengine_pcm_dma_complete;
	desc->callback_param = substream;
	prtd->cookie = dmaengine_submit(desc);
	return 0;
}

/**
 * snd_dmaengine_pcm_trigger - dmaengine based PCM trigger implementation
 * @substream: PCM substream
 * @cmd: Trigger command
 *
 * Returns 0 on success, a negative error code otherwise.
 *
 * This function can be used as the PCM trigger callback for dmaengine based PCM
 * driver implementations.
 */
int snd_dmaengine_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = dmaengine_pcm_prepare_and_submit(substream);
		if (ret) {
			printk("%s %d\n", __FUNCTION__, __LINE__);
			return ret;
		}
		dma_async_issue_pending(prtd->dma_chan);
		prtd->startflag = 1;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dmaengine_resume(prtd->dma_chan);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dmaengine_pause(prtd->dma_chan);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		dmaengine_terminate_all(prtd->dma_chan);
		prtd->startflag = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_trigger);

/**
 * snd_dmaengine_pcm_pointer_no_residue - dmaengine based PCM pointer implementation
 * @substream: PCM substream
 *
 * This function is deprecated and should not be used by new drivers, as its
 * results may be unreliable.
 */
snd_pcm_uframes_t snd_dmaengine_pcm_pointer_no_residue(struct snd_pcm_substream *substream)
{
	struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);
	return bytes_to_frames(substream->runtime, prtd->pos);
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_pointer_no_residue);

/**
 * snd_dmaengine_pcm_pointer - dmaengine based PCM pointer implementation
 * @substream: PCM substream
 *
 * This function can be used as the PCM pointer callback for dmaengine based PCM
 * driver implementations.
 */
snd_pcm_uframes_t snd_dmaengine_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);

	struct dma_tx_state state;
	enum dma_status status;
	unsigned int buf_size;
	unsigned int pos = 0;

	status = dmaengine_tx_status(prtd->dma_chan, prtd->cookie, &state);
	if (status == DMA_IN_PROGRESS || status == DMA_PAUSED) {
		buf_size = snd_pcm_lib_buffer_bytes(substream);
		if (state.residue > 0 && state.residue <= buf_size)
			pos = buf_size - state.residue;
	}

	return bytes_to_frames(substream->runtime, pos);
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_pointer);

/**
 * snd_dmaengine_pcm_request_channel - Request channel for the dmaengine PCM
 * @filter_fn: Filter function used to request the DMA channel
 * @filter_data: Data passed to the DMA filter function
 *
 * Returns NULL or the requested DMA channel.
 *
 * This function request a DMA channel for usage with dmaengine PCM.
 */
/*
struct dma_chan *snd_dmaengine_pcm_request_channel(dma_filter_fn filter_fn,
	void *filter_data)
{
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);

	return dai_dma_request_slave_channel(stream_type);
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_request_channel);
*/

/**
 * snd_dmaengine_pcm_open - Open a dmaengine based PCM substream
 * @substream: PCM substream
 * @chan: DMA channel to use for data transfers
 *
 * Returns 0 on success, a negative error code otherwise.
 *
 * The function should usually be called from the pcm open callback. Note that
 * this function will use private_data field of the substream's runtime. So it
 * is not availabe to your pcm driver implementation.
 */
int snd_dmaengine_pcm_open(struct snd_pcm_substream *substream,
	struct dma_chan *chan)
{
	struct dmaengine_pcm_runtime_data *prtd;
	int ret;

	if (!chan)
		return -ENXIO;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
	if (!prtd)
		return -ENOMEM;

	prtd->dma_chan = chan;

	substream->runtime->private_data = prtd;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_open);

/**
 * snd_dmaengine_pcm_open_request_chan - Open a dmaengine based PCM substream and request channel
 * @substream: PCM substream
 * @filter_fn: Filter function used to request the DMA channel
 * @filter_data: Data passed to the DMA filter function
 *
 * Returns 0 on success, a negative error code otherwise.
 *
 * This function will request a DMA channel using the passed filter function and
 * data. The function should usually be called from the pcm open callback. Note
 * that this function will use private_data field of the substream's runtime. So
 * it is not availabe to your pcm driver implementation.
 */
int snd_dmaengine_pcm_open_request_chan(struct snd_pcm_substream *substream,
	dma_filter_fn filter_fn, void *filter_data)
{
	/*
	return snd_dmaengine_pcm_open(substream,
		    snd_dmaengine_pcm_request_channel(filter_fn, filter_data, substream->stream));
	*/

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct s900_pcm_priv *pcm_priv
		= snd_soc_platform_get_drvdata(platform);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if(pcm_priv->output_mode == O_MODE_PCM)
		{
			return snd_dmaengine_pcm_open(substream, dai_dma_slave_pcm1tx_chan());
		}
		if (pcm_priv->output_mode == O_MODE_HDMI) {
			return snd_dmaengine_pcm_open(substream, dai_dma_slave_hdmia_chan());
		}

		return snd_dmaengine_pcm_open(substream, dai_dma_slave_tx_chan());
	} else {
		if(pcm_priv->input_mode == O_MODE_PCM)
		{
			return snd_dmaengine_pcm_open(substream, dai_dma_slave_pcm1rx_chan());
		}
		return snd_dmaengine_pcm_open(substream, dai_dma_slave_rx_chan());
	}
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_open_request_chan);

/**
 * snd_dmaengine_pcm_close - Close a dmaengine based PCM substream
 * @substream: PCM substream
 */
int snd_dmaengine_pcm_close(struct snd_pcm_substream *substream)
{
	struct dmaengine_pcm_runtime_data *prtd = substream_to_prtd(substream);

	kfree(prtd);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_close);

/**
 * snd_dmaengine_pcm_release_chan_close - Close a dmaengine based PCM substream and release channel
 * @substream: PCM substream
 *
 * Releases the DMA channel associated with the PCM substream.
 */
int snd_dmaengine_pcm_close_release_chan(struct snd_pcm_substream *substream)
{
	return snd_dmaengine_pcm_close(substream);
}
EXPORT_SYMBOL_GPL(snd_dmaengine_pcm_close_release_chan);

MODULE_LICENSE("GPL");
