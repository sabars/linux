#
# Makefile for the ft5406 tp driver
#

# tp
R_TP_MAX_POINT=10
R_TP_DOWNLOAD_FIRM=0
R_TP_UPGRADE_ID2=0x3
R_TP_I_FILE_NAME="NOVO10.1_SG5238_V31_20121219_60Hz_app.i"

 ifeq ($(R_TP_MAX_POINT),)
 R_TP_MAX_POINT = 5
 endif
 ifeq ($(R_TP_DOWNLOAD_FIRM),)
 R_TP_DOWNLOAD_FIRM = 0
 endif
 ifeq ($(R_TP_UPGRADE_ID2),)
 R_TP_UPGRADE_ID2 = 0x6
 endif
 ifeq ($(R_TP_I_FILE_NAME),)
 R_TP_I_FILE_NAME = "ft_app.i"
 endif

EXTRA_CFLAGS += -DFT5X06_MAX_POINT=${R_TP_MAX_POINT} -DFT5X0X_DOWNLOAD_FIRM=${R_TP_DOWNLOAD_FIRM} -DUPGRADE_ID2=${R_TP_UPGRADE_ID2} -DFIRM_I_FILE_NAME=${R_TP_I_FILE_NAME}

obj-$(CONFIG_TOUCHSCREEN_FT5X06)		  += ctp_ft5x06.o
ctp_ft5x06-objs := ft5x06.o