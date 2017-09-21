/*************************************************************************/ /*!
@File
@Title          Device specific start/stop routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific start/stop routines
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

/* The routines implemented here are built on top of an abstraction layer to
 * hide DDK/OS-specific details in case they are used outside of the DDK
 * (e.g. when trusted device is enabled).
 * Any new dependency should be added to rgxlayer_km.h. */
#include "rgxstartstop.h"

#if defined(SUPPORT_SHARED_SLC)
#include "rgxapi_km.h"
#include "rgxdevice.h"
#endif


#if !defined(FIX_HW_BRN_37453)
/*!
*******************************************************************************

 @Function      RGXEnableClocks

 @Description   Enable RGX Clocks

 @Input         hPrivate  : Implementation specific data

 @Return        void

******************************************************************************/
static void RGXEnableClocks(const void *hPrivate)
{
	RGXCommentLogPower(hPrivate, "RGX clock: use default (automatic clock gating)");
}
#endif


#if defined(RGX_FEATURE_META)
#if defined(PDUMP) || defined(SUPPORT_META_SLAVE_BOOT)
static PVRSRV_ERROR RGXWriteMetaRegThroughSP(const void *hPrivate, IMG_UINT32 ui32RegAddr, IMG_UINT32 ui32RegValue)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Wait for Slave Port to be Ready */
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_META_SP_MSLVCTRL1,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN);
	if (eError != PVRSRV_OK) return eError;

	/* Issue a Write */
	RGXWriteReg32(hPrivate, RGX_CR_META_SP_MSLVCTRL0, ui32RegAddr);
	RGXWriteReg32(hPrivate, RGX_CR_META_SP_MSLVDATAT, ui32RegValue);

	return eError;
}
#endif /* PDUMP || SUPPORT_META_SLAVE_BOOT */


static PVRSRV_ERROR RGXReadMetaRegThroughSP(const void *hPrivate,
                                            IMG_UINT32 ui32RegAddr,
                                            IMG_UINT32* ui32RegValue)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Wait for Slave Port to be Ready */
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_META_SP_MSLVCTRL1,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN);
	if (eError != PVRSRV_OK) return eError;

	/* Issue a Read */
	RGXWriteReg32(hPrivate, RGX_CR_META_SP_MSLVCTRL0, ui32RegAddr | RGX_CR_META_SP_MSLVCTRL0_RD_EN);

	/* Wait for Slave Port to be Ready */
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_META_SP_MSLVCTRL1,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN);
	if (eError != PVRSRV_OK) return eError;

#if !defined(NO_HARDWARE)
	*ui32RegValue = RGXReadReg32(hPrivate, RGX_CR_META_SP_MSLVDATAX);
#else
	*ui32RegValue = 0xFFFFFFFF;
#endif

	return eError;
}

#if defined (SUPPORT_META_SLAVE_BOOT)
static PVRSRV_ERROR RGXWriteMetaCoreRegThoughSP(const void *hPrivate,
                                                IMG_UINT32 ui32CoreReg,
                                                IMG_UINT32 ui32Value)
{
	IMG_UINT32 i = 0;

	RGXWriteMetaRegThroughSP(hPrivate, META_CR_TXUXXRXDT_OFFSET, ui32Value);
	RGXWriteMetaRegThroughSP(hPrivate, META_CR_TXUXXRXRQ_OFFSET, ui32CoreReg & ~META_CR_TXUXXRXRQ_RDnWR_BIT);

	do
	{
		RGXReadMetaRegThroughSP(hPrivate, META_CR_TXUXXRXRQ_OFFSET, &ui32Value);
	} while (((ui32Value & META_CR_TXUXXRXRQ_DREADY_BIT) != META_CR_TXUXXRXRQ_DREADY_BIT) && (i++ < 1000));

	if (i == 1000)
	{
		RGXCommentLogPower(hPrivate, "RGXWriteMetaCoreRegThoughSP: Timeout");
		return PVRSRV_ERROR_TIMEOUT;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR RGXStartFirmware(const void *hPrivate)
{
	PVRSRV_ERROR eError;

	/* Give privilege to debug and slave port */
	RGXWriteMetaRegThroughSP(hPrivate, META_CR_SYSC_JTAG_THREAD, META_CR_SYSC_JTAG_THREAD_PRIV_EN);

	/* Point Meta to the bootloader address, global (uncached) range */
	eError = RGXWriteMetaCoreRegThoughSP(hPrivate,
										 PC_ACCESS(0),
										 RGXFW_BOOTLDR_META_ADDR | META_MEM_GLOBAL_RANGE_BIT);

	if (eError != PVRSRV_OK)
	{
		RGXCommentLogPower(hPrivate, "RGXStart: RGX Firmware Slave boot Start failed!");
		return eError;
	}

	/* Enable minim encoding */
	RGXWriteMetaRegThroughSP(hPrivate, META_CR_TXPRIVEXT, META_CR_TXPRIVEXT_MINIM_EN);

	/* Enable Meta thread */
	RGXWriteMetaRegThroughSP(hPrivate, META_CR_T0ENABLE_OFFSET, META_CR_TXENABLE_ENABLE_BIT);

	return PVRSRV_OK;
}
#endif /* SUPPORT_META_SLAVE_BOOT */

#endif /* RGX_FEATURE_META */


#if defined(RGX_FEATURE_META)
/*!
*******************************************************************************

 @Function      RGXInitMetaProcWrapper

 @Description   Configures the hardware wrapper of the META processor

 @Input         hPrivate  : Implementation specific data

 @Return        void

******************************************************************************/
static void RGXInitMetaProcWrapper(const void *hPrivate)
{
	IMG_UINT64 ui64GartenConfig;

	/* Set Garten IDLE to META idle and Set the Garten Wrapper BIF Fence address */

	/* Garten IDLE bit controlled by META */
	ui64GartenConfig = RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META;

	/* The fence addr is set at the fw init sequence */

	/* Set PC = 0 for fences */
	ui64GartenConfig &= RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_PC_BASE_CLRMSK;

#if defined(RGX_FEATURE_SLC_VIVT)
#if !defined(FIX_HW_BRN_51281)
	/* Ensure the META fences go all the way to external memory */
	ui64GartenConfig |= RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_SLC_COHERENT_EN;    /* SLC Coherent 1 */
	ui64GartenConfig &= RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_PERSISTENCE_CLRMSK; /* SLC Persistence 0 */
#endif /* FIX_HW_BRN_51281 */

#else
	/* Set SLC DM=META */
	ui64GartenConfig |= ((IMG_UINT64) RGXFW_SEGMMU_META_DM_ID) << RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_FENCE_DM_SHIFT;

#endif /* RGX_FEATURE_SLC_VIVT */

	RGXCommentLogPower(hPrivate, "RGXStart: Configure META wrapper");
	RGXWriteReg64(hPrivate, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui64GartenConfig);
}

#else  /* RGX_FEATURE_META */

/*!
*******************************************************************************

 @Function      RGXInitMipsProcWrapper

 @Description   Configures the hardware wrapper of the MIPS processor

 @Input         hPrivate  : Implementation specific data

 @Return        void

******************************************************************************/
static void RGXInitMipsProcWrapper(const void *hPrivate)
{
	IMG_DEV_PHYADDR sPhyAddr;
	IMG_UINT64 ui64RemapSettings = RGXMIPSFW_BOOT_REMAP_LOG2_SEGMENT_SIZE; /* Same for all remap registers */

	RGXCommentLogPower(hPrivate, "RGXStart: Configure MIPS wrapper");

	/*
	 * MIPS wrapper (registers transaction ID and ISA mode) setup
	 */

	RGXAcquireGPURegsAddr(hPrivate, &sPhyAddr);

	RGXCommentLogPower(hPrivate, "RGXStart: Write wrapper config register");
	RGXMIPSWrapperConfig(hPrivate,
	                     RGX_CR_MIPS_WRAPPER_CONFIG,
	                     sPhyAddr.uiAddr,
	                     RGXMIPSFW_WRAPPER_CONFIG_REGBANK_ADDR_ALIGN,
	                     RGX_CR_MIPS_WRAPPER_CONFIG_BOOT_ISA_MODE_MICROMIPS);

	/*
	 * Boot remap setup
	 */

	RGXAcquireBootRemapAddr(hPrivate, &sPhyAddr);

#if defined(SUPPORT_TRUSTED_DEVICE)
	/* Remapped bootloader in secure memory */
	ui64RemapSettings |= RGX_CR_MIPS_ADDR_REMAP1_CONFIG2_TRUSTED_EN;
#endif

	RGXCommentLogPower(hPrivate, "RGXStart: Write boot remap registers");
	RGXBootRemapConfig(hPrivate,
	                   RGX_CR_MIPS_ADDR_REMAP1_CONFIG1,
	                   RGXMIPSFW_BOOT_REMAP_PHYS_ADDR_IN | RGX_CR_MIPS_ADDR_REMAP1_CONFIG1_MODE_ENABLE_EN,
	                   RGX_CR_MIPS_ADDR_REMAP1_CONFIG2,
	                   sPhyAddr.uiAddr,
	                   ~RGX_CR_MIPS_ADDR_REMAP1_CONFIG2_ADDR_OUT_CLRMSK,
	                   ui64RemapSettings);

	/*
	 * Data remap setup
	 */

	RGXAcquireDataRemapAddr(hPrivate, &sPhyAddr);

#if defined(SUPPORT_TRUSTED_DEVICE)
	/* Remapped data in non-secure memory */
	ui64RemapSettings &= RGX_CR_MIPS_ADDR_REMAP1_CONFIG2_TRUSTED_CLRMSK;
#endif

	RGXCommentLogPower(hPrivate, "RGXStart: Write data remap registers");
	RGXDataRemapConfig(hPrivate,
	                   RGX_CR_MIPS_ADDR_REMAP2_CONFIG1,
	                   RGXMIPSFW_DATA_REMAP_PHYS_ADDR_IN | RGX_CR_MIPS_ADDR_REMAP2_CONFIG1_MODE_ENABLE_EN,
	                   RGX_CR_MIPS_ADDR_REMAP2_CONFIG2,
	                   sPhyAddr.uiAddr,
	                   ~RGX_CR_MIPS_ADDR_REMAP2_CONFIG2_ADDR_OUT_CLRMSK,
	                   ui64RemapSettings);

	/*
	 * Code remap setup
	 */

	RGXAcquireCodeRemapAddr(hPrivate, &sPhyAddr);

#if defined(SUPPORT_TRUSTED_DEVICE)
	/* Remapped code in secure memory */
	ui64RemapSettings |= RGX_CR_MIPS_ADDR_REMAP1_CONFIG2_TRUSTED_EN;
#endif

	RGXCommentLogPower(hPrivate, "RGXStart: Write exceptions remap registers");
	RGXCodeRemapConfig(hPrivate,
	                   RGX_CR_MIPS_ADDR_REMAP3_CONFIG1,
	                   RGXMIPSFW_CODE_REMAP_PHYS_ADDR_IN | RGX_CR_MIPS_ADDR_REMAP3_CONFIG1_MODE_ENABLE_EN,
	                   RGX_CR_MIPS_ADDR_REMAP3_CONFIG2,
	                   sPhyAddr.uiAddr,
	                   ~RGX_CR_MIPS_ADDR_REMAP3_CONFIG2_ADDR_OUT_CLRMSK,
	                   ui64RemapSettings);

	/* Garten IDLE bit controlled by MIPS */
	RGXCommentLogPower(hPrivate, "RGXStart: Set GARTEN_IDLE type to MIPS");
	RGXWriteReg64(hPrivate, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META);

	/* Turn on the EJTAG probe (only useful driver live) */
	RGXWriteReg32(hPrivate, RGX_CR_MIPS_DEBUG_CONFIG, 0);
}
#endif /* RGX_FEATURE_META */


#if !defined(RGX_FEATURE_S7_CACHE_HIERARCHY)
#define RGX_INIT_SLC _RGXInitSLC

/*!
*******************************************************************************

 @Function      _RGXInitSLC

 @Description   Initialise RGX SLC

 @Input         hPrivate  : Implementation specific data

 @Return        void

******************************************************************************/
static void _RGXInitSLC(const void *hPrivate)
{
	IMG_UINT32 ui32Reg;
	IMG_UINT32 ui32RegVal;

#if defined(FIX_HW_BRN_36492)
	/* Because the WA for this BRN forbids using SLC reset, need to inval it instead */
	RGXCommentLogPower(hPrivate, "Invalidate the SLC");
	RGXWriteReg32(hPrivate, RGX_CR_SLC_CTRL_FLUSH_INVAL, RGX_CR_SLC_CTRL_FLUSH_INVAL_ALL_EN);

	/* Poll for completion */
	RGXPollReg32(hPrivate, RGX_CR_SLC_STATUS0, 0x0, RGX_CR_SLC_STATUS0_MASKFULL);
#endif

#if (RGX_FEATURE_SLC_SIZE_IN_BYTES < (128*1024))
	/*
	 * SLC Bypass control
	 */
	ui32Reg = RGX_CR_SLC_CTRL_BYPASS;
	ui32RegVal = RGX_CR_SLC_CTRL_BYPASS_REQ_TPU_EN;

	/* Bypass SLC for textures if the SLC size is less than 128kB */
	RGXCommentLogPower(hPrivate, "Bypass SLC for TPU");
	RGXWriteReg32(hPrivate, ui32Reg, ui32RegVal);
#endif

	/*
	 * SLC Bypass control
	 */
	ui32Reg = RGX_CR_SLC_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC_CTRL_MISC_ADDR_DECODE_MODE_PVR_HASH1;

	/* Bypass burst combiner if SLC line size is smaller than 1024 bits */
#if (RGX_FEATURE_SLC_CACHE_LINE_SIZE_BITS < 1024)
	ui32RegVal |= RGX_CR_SLC_CTRL_MISC_BYPASS_BURST_COMBINER_EN;
#endif

	RGXWriteReg32(hPrivate, ui32Reg, ui32RegVal);
}

#else /* RGX_FEATURE_S7_CACHE_HIERARCHY */
#define RGX_INIT_SLC _RGXInitSLC3

/*!
*******************************************************************************

 @Function      RGXInitSLC3

 @Description   Initialise RGX SLC3

 @Input         hPrivate  : Implementation specific data

 @Return        void

******************************************************************************/
static void _RGXInitSLC3(const void *hPrivate)
{
	IMG_UINT32 ui32Reg;
	IMG_UINT32 ui32RegVal;

#if defined(HW_ERN_51468)
	/*
	 * SLC control
	 */
	ui32Reg = RGX_CR_SLC3_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC3_CTRL_MISC_ADDR_DECODE_MODE_WEAVED_HASH;
	RGXWriteReg32(hPrivate, ui32Reg, ui32RegVal);
#else
	/*
	 * SLC control
	 */
	ui32Reg = RGX_CR_SLC3_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC3_CTRL_MISC_ADDR_DECODE_MODE_SCRAMBLE_PVR_HASH;
	RGXWriteReg32(hPrivate, ui32Reg, ui32RegVal);

	/*
	 * SLC scramble bits
	 */
	{
		IMG_UINT32 i;
		IMG_UINT32 aui32ScrambleRegs[] = {
		    RGX_CR_SLC3_SCRAMBLE,
		    RGX_CR_SLC3_SCRAMBLE2,
		    RGX_CR_SLC3_SCRAMBLE3,
		    RGX_CR_SLC3_SCRAMBLE4
		};

		IMG_UINT64 aui64ScrambleValues[] = {
#if (RGX_FEATURE_SLC_BANKS == 2)
		   IMG_UINT64_C(0x6965a99a55696a6a),
		   IMG_UINT64_C(0x6aa9aa66959aaa9a),
		   IMG_UINT64_C(0x9a5665965a99a566),
		   IMG_UINT64_C(0x5aa69596aa66669a)
#elif (RGX_FEATURE_SLC_BANKS == 4)
		   IMG_UINT64_C(0xc6788d722dd29ce4),
		   IMG_UINT64_C(0x7272e4e11b279372),
		   IMG_UINT64_C(0x87d872d26c6c4be1),
		   IMG_UINT64_C(0xe1b4878d4b36e478)
#elif (RGX_FEATURE_SLC_BANKS == 8)
		   IMG_UINT64_C(0x859d6569e8fac688),
		   IMG_UINT64_C(0xf285e1eae4299d33),
		   IMG_UINT64_C(0x1e1af2be3c0aa447)
#endif
		};

		for (i = 0; i < sizeof(aui64ScrambleValues)/sizeof(IMG_UINT64); i++)
		{
			IMG_UINT32 ui32Reg = aui32ScrambleRegs[i];
			IMG_UINT64 ui64Value = aui64ScrambleValues[i];

			RGXWriteReg64(hPrivate, ui32Reg, ui64Value);
		}
	}
#endif

#if defined(HW_ERN_45914)
	/* Disable the forced SLC coherency which the hardware enables for compatibility with older pdumps */
	RGXCommentLogPower(hPrivate, "RGXStart: disable forced SLC coherency");
	RGXWriteReg64(hPrivate, RGX_CR_GARTEN_SLC, 0);
#endif
}
#endif /* RGX_FEATURE_S7_CACHE_HIERARCHY */


/*!
*******************************************************************************

 @Function      RGXInitBIF

 @Description   Initialise RGX BIF

 @Input         hPrivate : Implementation specific data

 @Return        void

******************************************************************************/
static void RGXInitBIF(const void *hPrivate)
{
#if !defined(RGX_FEATURE_MIPS)
	IMG_DEV_PHYADDR sPCAddr;

	/*
	 * Acquire the address of the Kernel Page Catalogue.
	 */
	RGXAcquireKernelMMUPC(hPrivate, &sPCAddr);

	/*
	 * Write the kernel catalogue base.
	 */
	RGXCommentLogPower(hPrivate, "RGX firmware MMU Page Catalogue");

#if !defined(RGX_FEATURE_SLC_VIVT)
	/* Write the cat-base address */
	RGXWriteKernelMMUPC64(hPrivate,
	                      RGX_CR_BIF_CAT_BASE0,
	                      RGX_CR_BIF_CAT_BASE0_ADDR_ALIGNSHIFT,
	                      RGX_CR_BIF_CAT_BASE0_ADDR_SHIFT,
	                      ((sPCAddr.uiAddr
	                      >> RGX_CR_BIF_CAT_BASE0_ADDR_ALIGNSHIFT)
	                      << RGX_CR_BIF_CAT_BASE0_ADDR_SHIFT)
	                      & ~RGX_CR_BIF_CAT_BASE0_ADDR_CLRMSK);
#else
	/* Set the mapping context */
	RGXWriteReg32(hPrivate, RGX_CR_MMU_CBASE_MAPPING_CONTEXT, 0);

	/* Write the cat-base address */
	RGXWriteKernelMMUPC32(hPrivate,
	                      RGX_CR_MMU_CBASE_MAPPING,
	                      RGX_CR_MMU_CBASE_MAPPING_BASE_ADDR_ALIGNSHIFT,
	                      RGX_CR_MMU_CBASE_MAPPING_BASE_ADDR_SHIFT,
	                      (IMG_UINT32)(((sPCAddr.uiAddr
	                      >> RGX_CR_MMU_CBASE_MAPPING_BASE_ADDR_ALIGNSHIFT)
	                      << RGX_CR_MMU_CBASE_MAPPING_BASE_ADDR_SHIFT)
	                      & ~RGX_CR_MMU_CBASE_MAPPING_BASE_ADDR_CLRMSK));
#endif /* RGX_FEATURE_SLC_VIVT */

#endif /* RGX_FEATURE_MIPS */

	/*
	 * Trusted Firmware boot
	 */
#if defined(SUPPORT_TRUSTED_DEVICE)
	RGXCommentLogPower(hPrivate, "RGXInitBIF: Trusted Device enabled");
	RGXWriteReg32(hPrivate, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN);
#endif
}


#if defined(RGX_FEATURE_AXI_ACELITE)
/*!
*******************************************************************************

 @Function      RGXAXIACELiteInit

 @Description   Initialise AXI-ACE Lite interface

 @Input         hPrivate : Implementation specific data

 @Return        void

******************************************************************************/
static void RGXAXIACELiteInit(const void *hPrivate)
{
	IMG_UINT32 ui32RegAddr;
	IMG_UINT64 ui64RegVal;

	ui32RegAddr = RGX_CR_AXI_ACE_LITE_CONFIGURATION;

	/* Setup AXI-ACE config. Set everything to outer cache */
	ui64RegVal = (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT) |
	             (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT) |
	             (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT)  |
	             (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT) |
	             (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT) |
#if defined(FIX_HW_BRN_42321)
	             (((IMG_UINT64) 1) << RGX_CR_AXI_ACE_LITE_CONFIGURATION_DISABLE_COHERENT_WRITELINEUNIQUE_SHIFT) |
#endif
	             (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT) |
	             (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT) |
				 (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT) |
				 (1U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_NON_SNOOPING_SHIFT);

	RGXCommentLogPower(hPrivate, "Init AXI-ACE interface");
	RGXWriteReg64(hPrivate, ui32RegAddr, ui64RegVal);
}
#endif


PVRSRV_ERROR RGXStart(const void *hPrivate)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

#if defined(RGX_FEATURE_SYS_BUS_SECURE_RESET)
	/* Disable the default sys_bus_secure protection to perform minimal setup */
	RGXCommentLogPower(hPrivate, "RGXStart: Disable sys_bus_secure");
	RGXWriteReg32(hPrivate, RGX_CR_SYS_BUS_SECURE, 0);
#endif

#if defined(FIX_HW_BRN_37453)
	/* Force all clocks on*/
	RGXCommentLogPower(hPrivate, "RGXStart: force all clocks on");
	RGXWriteReg64(hPrivate, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_ON);
#endif

#if defined(SUPPORT_SHARED_SLC) && !defined(FIX_HW_BRN_36492)
	/* When the SLC is shared, the SLC reset is performed by the System layer when calling
	 * RGXInitSLC (before any device uses it), therefore mask out the SLC bit to avoid
	 * soft_resetting it here. If HW_BRN_36492, the bit is already masked out.
	 */
#define RGX_CR_SOFT_RESET_ALL  (RGX_CR_SOFT_RESET_MASKFULL ^ RGX_CR_SOFT_RESET_SLC_EN)
	RGXCommentLogPower(hPrivate, "RGXStart: Shared SLC (don't reset SLC as part of RGX reset)");
#else
#define RGX_CR_SOFT_RESET_ALL  (RGX_CR_SOFT_RESET_MASKFULL)
#endif

#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	/* Set RGX in soft-reset */
	RGXCommentLogPower(hPrivate, "RGXStart: soft reset assert step 1");
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_DUSTS);

	RGXCommentLogPower(hPrivate, "RGXStart: soft reset assert step 2");
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_JONES_ALL | RGX_S7_SOFT_RESET_DUSTS);
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET2, RGX_S7_SOFT_RESET2);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) RGXReadReg64(hPrivate, RGX_CR_SOFT_RESET);

	/* Take everything out of reset but META/MIPS */
	RGXCommentLogPower(hPrivate, "RGXStart: soft reset de-assert step 1 excluding %s", RGXFW_PROCESSOR);
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_S7_SOFT_RESET_DUSTS | RGX_CR_SOFT_RESET_GARTEN_EN);
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET2, 0x0);

	(void) RGXReadReg64(hPrivate, RGX_CR_SOFT_RESET);

	RGXCommentLogPower(hPrivate, "RGXStart: soft reset de-assert step 2 excluding %s", RGXFW_PROCESSOR);
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN);

	(void) RGXReadReg64(hPrivate, RGX_CR_SOFT_RESET);
#else
	/* Set RGX in soft-reset */
	RGXCommentLogPower(hPrivate, "RGXStart: soft reset everything");
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_ALL);

	/* Take Rascal and Dust out of reset */
	RGXCommentLogPower(hPrivate, "RGXStart: Rascal and Dust out of reset");
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_ALL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) RGXReadReg64(hPrivate, RGX_CR_SOFT_RESET);

	/* Take everything out of reset but META/MIPS */
	RGXCommentLogPower(hPrivate, "RGXStart: Take everything out of reset but %s", RGXFW_PROCESSOR);
	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN);
#endif

#if !defined(FIX_HW_BRN_37453)
	/* Enable clocks */
	RGXEnableClocks(hPrivate);
#endif

	/*
	 * Initialise SLC.
	 */
#if !defined(SUPPORT_SHARED_SLC)
	RGX_INIT_SLC(hPrivate);
#endif

#if defined(RGX_FEATURE_META)
#if defined(SUPPORT_META_SLAVE_BOOT)
	/* Configure META to Slave boot */
	RGXCommentLogPower(hPrivate, "RGXStart: META Slave boot");
	RGXWriteReg32(hPrivate, RGX_CR_META_BOOT, 0);
#else
	/* Configure META to Master boot */
	RGXCommentLogPower(hPrivate, "RGXStart: META Master boot");
	RGXWriteReg32(hPrivate, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN);
#endif
#endif

	/*
	 * Initialise Firmware wrapper
	 */
#if defined(RGX_FEATURE_META)
	RGXInitMetaProcWrapper(hPrivate);
#else
	RGXInitMipsProcWrapper(hPrivate);
#endif

#if defined(RGX_FEATURE_AXI_ACELITE)
	/* We must init the AXI-ACE interface before 1st BIF transaction */
	RGXAXIACELiteInit(hPrivate);
#endif

	/*
	 * Initialise BIF.
	 */
	RGXInitBIF(hPrivate);

	RGXCommentLogPower(hPrivate, "RGXStart: Take %s out of reset", RGXFW_PROCESSOR);

	/* Need to wait for at least 16 cycles before taking META/MIPS out of reset ... */
	RGXWaitCycles(hPrivate, 32, 3);

	RGXWriteReg64(hPrivate, RGX_CR_SOFT_RESET, 0x0);
	(void) RGXReadReg64(hPrivate, RGX_CR_SOFT_RESET);

	/* ... and afterwards */
	RGXWaitCycles(hPrivate, 32, 3);

#if defined(FIX_HW_BRN_37453)
	/* We rely on the 32 clk sleep from above */

	/* Switch clocks back to auto */
	RGXCommentLogPower(hPrivate, "RGXStart: set clocks back to auto");
	RGXWriteReg64(hPrivate, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_AUTO);
#endif

#if defined(RGX_FEATURE_META) && defined(SUPPORT_META_SLAVE_BOOT)
	RGXCommentLogPower(hPrivate, "RGXStart: RGX Firmware Slave boot Start");
	eError = RGXStartFirmware(hPrivate);
	if (eError != PVRSRV_OK) return eError;
#else
	RGXCommentLogPower(hPrivate, "RGXStart: RGX Firmware Master boot Start");
#endif

	/* Enable Sys Bus security */
#if defined(SUPPORT_TRUSTED_DEVICE)
	RGXCommentLogPower(hPrivate, "RGXStart: Enable sys_bus_secure");
	RGXWriteReg32(hPrivate, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN);
#endif

	return eError;
}


PVRSRV_ERROR RGXStop(const void *hPrivate)
{
	PVRSRV_ERROR eError;

	/* Wait for Sidekick/Jones to signal IDLE except for the Garten Wrapper */
#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SIDEKICK_IDLE,
	                      RGX_CR_SIDEKICK_IDLE_MASKFULL^(RGX_CR_SIDEKICK_IDLE_GARTEN_EN|RGX_CR_SIDEKICK_IDLE_SOCIF_EN|RGX_CR_SIDEKICK_IDLE_HOSTIF_EN),
	                      RGX_CR_SIDEKICK_IDLE_MASKFULL^(RGX_CR_SIDEKICK_IDLE_GARTEN_EN|RGX_CR_SIDEKICK_IDLE_SOCIF_EN|RGX_CR_SIDEKICK_IDLE_HOSTIF_EN));
#else
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_JONES_IDLE,
	                      RGX_CR_JONES_IDLE_MASKFULL^(RGX_CR_JONES_IDLE_GARTEN_EN|RGX_CR_JONES_IDLE_SOCIF_EN|RGX_CR_JONES_IDLE_HOSTIF_EN),
	                      RGX_CR_JONES_IDLE_MASKFULL^(RGX_CR_JONES_IDLE_GARTEN_EN|RGX_CR_JONES_IDLE_SOCIF_EN|RGX_CR_JONES_IDLE_HOSTIF_EN));
#endif
	if (eError != PVRSRV_OK) return eError;


#if !defined(SUPPORT_SHARED_SLC)
	/* Wait for SLC to signal IDLE */
#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SLC_IDLE,
	                      RGX_CR_SLC_IDLE_MASKFULL,
	                      RGX_CR_SLC_IDLE_MASKFULL);
#else
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SLC3_IDLE,
	                      RGX_CR_SLC3_IDLE_MASKFULL,
	                      RGX_CR_SLC3_IDLE_MASKFULL);
#endif /* RGX_FEATURE_S7_TOP_INFRASTRUCTURE */
#endif /* SUPPORT_SHARED_SLC */
	if (eError != PVRSRV_OK) return eError;


	/* Unset MTS DM association with threads */
	RGXWriteReg32(hPrivate,
	              RGX_CR_MTS_INTCTX_THREAD0_DM_ASSOC,
	              RGX_CR_MTS_INTCTX_THREAD0_DM_ASSOC_DM_ASSOC_CLRMSK
	              & RGX_CR_MTS_INTCTX_THREAD0_DM_ASSOC_MASKFULL);
	RGXWriteReg32(hPrivate,
	              RGX_CR_MTS_BGCTX_THREAD0_DM_ASSOC,
	              RGX_CR_MTS_BGCTX_THREAD0_DM_ASSOC_DM_ASSOC_CLRMSK
	              & RGX_CR_MTS_BGCTX_THREAD0_DM_ASSOC_MASKFULL);
	RGXWriteReg32(hPrivate,
	              RGX_CR_MTS_INTCTX_THREAD1_DM_ASSOC,
	              RGX_CR_MTS_INTCTX_THREAD1_DM_ASSOC_DM_ASSOC_CLRMSK
	              & RGX_CR_MTS_INTCTX_THREAD1_DM_ASSOC_MASKFULL);
	RGXWriteReg32(hPrivate,
	              RGX_CR_MTS_BGCTX_THREAD1_DM_ASSOC,
	              RGX_CR_MTS_BGCTX_THREAD1_DM_ASSOC_DM_ASSOC_CLRMSK
	              & RGX_CR_MTS_BGCTX_THREAD1_DM_ASSOC_MASKFULL);


#if defined(RGX_FEATURE_META) && defined(PDUMP)
	/* Disabling threads is only required for pdumps to stop the fw gracefully */

	/* Disable thread 0 */
	eError = RGXWriteMetaRegThroughSP(hPrivate,
	                                  META_CR_T0ENABLE_OFFSET,
	                                  ~META_CR_TXENABLE_ENABLE_BIT);
	if (eError != PVRSRV_OK) return eError;

	/* Disable thread 1 */
	eError = RGXWriteMetaRegThroughSP(hPrivate,
	                                  META_CR_T1ENABLE_OFFSET,
	                                  ~META_CR_TXENABLE_ENABLE_BIT);
	if (eError != PVRSRV_OK) return eError;

	/* Clear down any irq raised by META (done after disabling the FW
	 * threads to avoid a race condition).
	 * This is only really needed for PDumps but we do it anyway driver-live.
	 */
	RGXWriteReg32(hPrivate, RGX_CR_META_SP_MSLVIRQSTATUS, 0x0);

	/* Wait for the Slave Port to finish all the transactions */
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_META_SP_MSLVCTRL1,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN | RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
	                      RGX_CR_META_SP_MSLVCTRL1_READY_EN | RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN);
	if (eError != PVRSRV_OK) return eError;
#endif


	/* Extra Idle checks */
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_BIF_STATUS_MMU,
	                      0,
	                      RGX_CR_BIF_STATUS_MMU_MASKFULL);
	if (eError != PVRSRV_OK) return eError;

	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_BIFPM_STATUS_MMU,
	                      0,
	                      RGX_CR_BIFPM_STATUS_MMU_MASKFULL);
	if (eError != PVRSRV_OK) return eError;

#if !defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE) && !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_BIF_READS_EXT_STATUS,
	                      0,
	                      RGX_CR_BIF_READS_EXT_STATUS_MASKFULL);
	if (eError != PVRSRV_OK) return eError;
#endif

	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_BIFPM_READS_EXT_STATUS,
	                      0,
	                      RGX_CR_BIFPM_READS_EXT_STATUS_MASKFULL);
	if (eError != PVRSRV_OK) return eError;

	eError = RGXPollReg64(hPrivate,
	                      RGX_CR_SLC_STATUS1,
	                      0,
#if defined(FIX_HW_BRN_43276)
	                      RGX_CR_SLC_STATUS1_MASKFULL & RGX_CR_SLC_STATUS1_READS1_EXT_CLRMSK & RGX_CR_SLC_STATUS1_READS0_EXT_CLRMSK);
#else
	                      RGX_CR_SLC_STATUS1_MASKFULL);
#endif
	if (eError != PVRSRV_OK) return eError;

#if defined(RGX_FEATURE_SLC_BANKS) && (RGX_FEATURE_SLC_BANKS == 4)
	eError = RGXPollReg64(hPrivate,
	                      RGX_CR_SLC_STATUS2,
	                      0,
	                      RGX_CR_SLC_STATUS2_MASKFULL);
	if (eError != PVRSRV_OK) return eError;
#endif


#if !defined(SUPPORT_SHARED_SLC)
	/* Wait for SLC to signal IDLE */
#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SLC_IDLE,
	                      RGX_CR_SLC_IDLE_MASKFULL,
	                      RGX_CR_SLC_IDLE_MASKFULL);
#else
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SLC3_IDLE,
	                      RGX_CR_SLC3_IDLE_MASKFULL,
	                      RGX_CR_SLC3_IDLE_MASKFULL);
#endif /* RGX_FEATURE_S7_TOP_INFRASTRUCTURE */
#endif /* SUPPORT_SHARED_SLC */
	if (eError != PVRSRV_OK) return eError;


	/* Wait for Sidekick/Jones to signal IDLE except for the Garten Wrapper */
#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SIDEKICK_IDLE,
	                      RGX_CR_SIDEKICK_IDLE_MASKFULL^(RGX_CR_SIDEKICK_IDLE_GARTEN_EN|RGX_CR_SIDEKICK_IDLE_SOCIF_EN|RGX_CR_SIDEKICK_IDLE_HOSTIF_EN),
	                      RGX_CR_SIDEKICK_IDLE_MASKFULL^(RGX_CR_SIDEKICK_IDLE_GARTEN_EN|RGX_CR_SIDEKICK_IDLE_SOCIF_EN|RGX_CR_SIDEKICK_IDLE_HOSTIF_EN));
#else
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_JONES_IDLE,
	                      RGX_CR_JONES_IDLE_MASKFULL^(RGX_CR_JONES_IDLE_GARTEN_EN|RGX_CR_JONES_IDLE_SOCIF_EN|RGX_CR_JONES_IDLE_HOSTIF_EN),
	                      RGX_CR_JONES_IDLE_MASKFULL^(RGX_CR_JONES_IDLE_GARTEN_EN|RGX_CR_JONES_IDLE_SOCIF_EN|RGX_CR_JONES_IDLE_HOSTIF_EN));
#endif
	if (eError != PVRSRV_OK) return eError;


#if defined(RGX_FEATURE_META)
	{
		IMG_UINT32 ui32RegValue;

		eError = RGXReadMetaRegThroughSP(hPrivate,
		                                 META_CR_TxVECINT_BHALT,
		                                 &ui32RegValue);
		if (eError != PVRSRV_OK) return eError;

		if ((ui32RegValue & 0xFFFFFFFFU) == 0x0)
		{
			/* Wait for Sidekick/Jones to signal IDLE including
			 * the Garten Wrapper if there is no debugger attached
			 * (TxVECINT_BHALT = 0x0) */
#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
			eError = RGXPollReg32(hPrivate,
			                      RGX_CR_SIDEKICK_IDLE,
			                      RGX_CR_SIDEKICK_IDLE_GARTEN_EN,
			                      RGX_CR_SIDEKICK_IDLE_GARTEN_EN);
			if (eError != PVRSRV_OK) return eError;
#else
			eError = RGXPollReg32(hPrivate,
			                      RGX_CR_JONES_IDLE,
			                      RGX_CR_JONES_IDLE_GARTEN_EN,
			                      RGX_CR_JONES_IDLE_GARTEN_EN);
			if (eError != PVRSRV_OK) return eError;
#endif /* RGX_FEATURE_S7_TOP_INFRASTRUCTURE */
		}
	}
#else /* defined(RGX_FEATURE_META) */
	eError = RGXPollReg32(hPrivate,
	                      RGX_CR_SIDEKICK_IDLE,
	                      RGX_CR_SIDEKICK_IDLE_GARTEN_EN,
	                      RGX_CR_SIDEKICK_IDLE_GARTEN_EN);
	if (eError != PVRSRV_OK) return eError;
#endif /* RGX_FEATURE_META */

	return eError;
}


/*
 * RGXInitSLC
 */
#if defined(SUPPORT_SHARED_SLC)
PVRSRV_ERROR RGXInitSLC(IMG_HANDLE hDevHandle)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO *psDevInfo;
	void *pvPowerParams;

	if (psDeviceNode == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psDevInfo = psDeviceNode->pvDevice;
	pvPowerParams = &psDevInfo->sPowerParams;

#if !defined(FIX_HW_BRN_36492)
	/* reset the SLC */
	RGXCommentLogPower(pvPowerParams, "RGXInitSLC: soft reset SLC");
	RGXWriteReg64(pvPowerParams, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_SLC_EN);

	/* Read soft-reset to fence previous write in order to clear the SOCIF pipeline */
	(void) RGXReadReg64(pvPowerParams, RGX_CR_SOFT_RESET);

	/* Take everything out of reset */
	RGXWriteReg64(pvPowerParams, RGX_CR_SOFT_RESET, 0x0);
#endif

	RGX_INIT_SLC(pvPowerParams);

	return PVRSRV_OK;
}
#endif

