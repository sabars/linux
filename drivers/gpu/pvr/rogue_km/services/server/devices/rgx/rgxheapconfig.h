/*************************************************************************/ /*!
@File
@Title          device configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Memory heaps device specific configuration
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

//#warning FIXME:  add the MMU specialisation defines here (or in hwdefs, perhaps?)

#ifndef __RGXHEAPCONFIG_H__
#define __RGXHEAPCONFIG_H__

#include "rgxdefs_km.h"

/*      
	RGX Device Virtual Address Space Definitions:

	Notes:
	Base addresses have to be a multiple of 4MiB
	
	RGX_PDSCODEDATA_HEAP_BASE and RGX_USCCODE_HEAP_BASE will be programmed, on a
	global basis, into RGX_CR_PDS_EXEC_BASE and RGX_CR_USC_CODE_BASE_*
	respectively. Therefore if clients use multiple configs they must still be 
	consistent with	their definitions for these heaps.

	Shared virtual memory (SVM) support requires half of the address space to
	be reserved for SVM allocations unless BRN fixes are required in which
	case the SVM heap is disabled.
*/

#if RGX_FEATURE_VIRTUAL_ADDRESS_SPACE_BITS == 40

#if defined(FIX_HW_BRN_52402) || defined(FIX_HW_BRN_55091)
	/* Start at 0. Size of 0 (heap disabled due to BRNs) */
	#define RGX_GENERAL_SVM_HEAP_BASE			IMG_UINT64_C(0x0000000000)
    #define RGX_GENERAL_SVM_HEAP_SIZE			IMG_UINT64_C(0x0000000000)
#else
	/* Start at 4 MiB Size of 512 GiB less 4 MiB (managed by OS/Services) */
	#define RGX_GENERAL_SVM_HEAP_BASE			IMG_UINT64_C(0x0000400000)
    #define RGX_GENERAL_SVM_HEAP_SIZE			IMG_UINT64_C(0x7FFFC00000)
#endif

#if defined(FIX_HW_BRN_52402) || defined(FIX_HW_BRN_55091)
	/* Start at 16GiB. Size of 512 GiB */
	#define RGX_GENERAL_HEAP_BASE				IMG_UINT64_C(0x0400000000)
    #define RGX_GENERAL_HEAP_SIZE				IMG_UINT64_C(0x8000000000)
#else
	/* Start at 512GiB. Size of 128 GiB */
	#define RGX_GENERAL_HEAP_BASE				IMG_UINT64_C(0x8000000000)
    #define RGX_GENERAL_HEAP_SIZE				IMG_UINT64_C(0x2000000000)
#endif

	/* start at 664 GiB. Size of 32 GiB */
	#define RGX_BIF_TILING_NUM_HEAPS            4
	#define RGX_BIF_TILING_HEAP_SIZE            IMG_UINT64_C(0x0200000000)
	#define RGX_BIF_TILING_HEAP_1_BASE          IMG_UINT64_C(0xA600000000)
	#define RGX_BIF_TILING_HEAP_2_BASE          (RGX_BIF_TILING_HEAP_1_BASE + RGX_BIF_TILING_HEAP_SIZE)
	#define RGX_BIF_TILING_HEAP_3_BASE          (RGX_BIF_TILING_HEAP_2_BASE + RGX_BIF_TILING_HEAP_SIZE)
	#define RGX_BIF_TILING_HEAP_4_BASE          (RGX_BIF_TILING_HEAP_3_BASE + RGX_BIF_TILING_HEAP_SIZE)

#if defined(FIX_HW_BRN_52402)
	/* HWBRN52402 workaround requires PDS memory to be below 16GB. Start at 8GB. Size of 4GB. */
	#define RGX_PDSCODEDATA_HEAP_BASE			IMG_UINT64_C(0x0200000000)
    #define RGX_PDSCODEDATA_HEAP_SIZE			IMG_UINT64_C(0x0100000000)
#else
	/* Start at 700GiB. Size of 4 GiB */
	#define RGX_PDSCODEDATA_HEAP_BASE			IMG_UINT64_C(0xAF00000000)
    #define RGX_PDSCODEDATA_HEAP_SIZE			IMG_UINT64_C(0x0100000000)
#endif

	/* start at 704GiB, size of 1 MiB */
	#define RGX_VISTEST_HEAP_BASE				IMG_UINT64_C(0xB000000000)
    #define RGX_VISTEST_HEAP_SIZE				IMG_UINT64_C(0x0000100000)
 
#if defined(FIX_HW_BRN_52402)
	/* HWBRN52402 workaround requires PDS memory to be below 16GB. Start at 12GB. Size of 4GB. */
	#define RGX_USCCODE_HEAP_BASE				IMG_UINT64_C(0x0300000000)
    #define RGX_USCCODE_HEAP_SIZE				IMG_UINT64_C(0x0100000000)
#else
	/* Start at 800GiB. Size of 4 GiB */
	#define RGX_USCCODE_HEAP_BASE				IMG_UINT64_C(0xC800000000)
    #define RGX_USCCODE_HEAP_SIZE				IMG_UINT64_C(0x0100000000)
#endif
 
	/* Start at 903GiB. Size of 24 MB for META and 32MB for MIPS */
	#define RGX_FIRMWARE_HEAP_BASE				IMG_UINT64_C(0xE1C0000000)
#if defined(SUPPORT_PVRSRV_GPUVIRT)
	#define RGX_FIRMWARE_HEAP_SIZE				PVRSRV_GPUVIRT_FWHEAP_SIZE
#else
	#if defined(RGX_FEATURE_META)
		#define RGX_FIRMWARE_HEAP_SIZE			3*RGXFW_SEGMMU_DMAP_SIZE
	#else
		#define RGX_FIRMWARE_HEAP_SIZE			IMG_UINT64_C(0x0002000000)
	#endif
#endif

#if defined(FIX_HW_BRN_52402) || defined(FIX_HW_BRN_55091)
	/* HWBRN52402 & HWBRN55091 workarounds requires TQ memory to be below 16GB and 16GB aligned. Start at 0GB. Size of 8GB. */
    #define RGX_TQ3DPARAMETERS_HEAP_BASE		IMG_UINT64_C(0x0000000000)
    #define RGX_TQ3DPARAMETERS_HEAP_SIZE		IMG_UINT64_C(0x0200000000)
#else
	/* Start at 912GiB. Size of 16 GiB. 16GB aligned to match RGX_CR_ISP_PIXEL_BASE */
    #define RGX_TQ3DPARAMETERS_HEAP_BASE		IMG_UINT64_C(0xE400000000)
    #define RGX_TQ3DPARAMETERS_HEAP_SIZE		IMG_UINT64_C(0x0400000000)
#endif

	/* Start at 928GiB. Size of 4 GiB */
	#define RGX_DOPPLER_HEAP_BASE				IMG_UINT64_C(0xE800000000)
	#define RGX_DOPPLER_HEAP_SIZE				IMG_UINT64_C(0x0100000000)

	/* Start at 932GiB. Size of 4 GiB */
	#define RGX_DOPPLER_OVERFLOW_HEAP_BASE		IMG_UINT64_C(0xE900000000)
	#define RGX_DOPPLER_OVERFLOW_HEAP_SIZE		IMG_UINT64_C(0x0100000000)

	/* Start at 936GiB. Size of 256 KBytes */
	#define RGX_SIGNALS_HEAP_BASE				IMG_UINT64_C(0xEA00000000)
	#define RGX_SIGNALS_HEAP_SIZE				IMG_UINT64_C(0x0000040000)

	/* TDM TPU YUV coeffs - can be reduced to a single page */
	#define RGX_TDM_TPU_YUV_COEFFS_HEAP_BASE	IMG_UINT64_C(0xEA00080000)
	#define RGX_TDM_TPU_YUV_COEFFS_HEAP_SIZE	IMG_UINT64_C(0x0000040000)

	/* Size of 16 * 4 KB (think about large page systems) */
#if defined(FIX_HW_BRN_37200)
    #define RGX_HWBRN37200_HEAP_BASE			IMG_UINT64_C(0xFFFFF00000)
    #define RGX_HWBRN37200_HEAP_SIZE			IMG_UINT64_C(0x0000100000)
#endif

	/* signal we've identified the core by the build */
	#define RGX_CORE_IDENTIFIED
#endif /* RGX_FEATURE_VIRTUAL_ADDRESS_SPACE_BITS == 40 */

#if !defined(RGX_CORE_IDENTIFIED)
	#error "rgxheapconfig.h: ERROR: unspecified RGX Core version"
#endif

#endif /* __RGXHEAPCONFIG_H__ */

/*****************************************************************************
 End of file (rgxheapconfig.h)
*****************************************************************************/