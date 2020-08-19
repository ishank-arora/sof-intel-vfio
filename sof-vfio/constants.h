#ifndef __CONSTANTS_H
#define __CONSTANTS_H

#include <stdbool.h>
#include <linux/const.h>
#include <sound/asound.h>


#define BIT(nr)			(1UL << (nr))

#define PAGE_SIZE 4096

#define BITS_PER_LONG 64

#define _UL(x) _BITUL(x)

#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))


#define SND_SOF_BARS	8

/* PCI registers */
#define PCI_TCSEL			0x44
#define PCI_PGCTL			PCI_TCSEL
#define PCI_CGCTL			0x48

/* PCI_PGCTL bits */
#define PCI_PGCTL_ADSPPGD               BIT(2)
#define PCI_PGCTL_LSRMD_MASK		BIT(4)

/* PCI_CGCTL bits */
#define PCI_CGCTL_MISCBDCGE_MASK	BIT(6)
#define PCI_CGCTL_ADSPDCGE              BIT(1)

/* Legacy HDA registers and bits used - widths are variable */
#define SOF_HDA_GCAP			0x0
#define SOF_HDA_GCTL			0x8
/* accept unsol. response enable */
#define SOF_HDA_GCTL_UNSOL		BIT(8)
#define SOF_HDA_LLCH			0x14
#define SOF_HDA_INTCTL			0x20
#define SOF_HDA_INTSTS			0x24
#define SOF_HDA_WAKESTS			0x0E
#define SOF_HDA_WAKESTS_INT_MASK	((1 << 8) - 1)
#define SOF_HDA_RIRBSTS			0x5d

/* SOF_HDA_GCTL register bist */
#define SOF_HDA_GCTL_RESET		BIT(0)

/* SOF_HDA_INCTL and SOF_HDA_INTSTS regs */
#define SOF_HDA_INT_GLOBAL_EN		BIT(31)
#define SOF_HDA_INT_CTRL_EN		BIT(30)
#define SOF_HDA_INT_ALL_STREAM		0xff

#define SOF_HDA_MAX_CAPS		10
#define SOF_HDA_CAP_ID_OFF		16
#define SOF_HDA_CAP_ID_MASK		GENMASK(SOF_HDA_CAP_ID_OFF + 11,\
						SOF_HDA_CAP_ID_OFF)
#define SOF_HDA_CAP_NEXT_MASK		0xFFFF

#define SOF_HDA_GTS_CAP_ID			0x1
#define SOF_HDA_ML_CAP_ID			0x2

#define SOF_HDA_PP_CAP_ID		0x3
#define SOF_HDA_REG_PP_PPCH		0x10
#define SOF_HDA_REG_PP_PPCTL		0x04
#define SOF_HDA_REG_PP_PPSTS		0x08
#define SOF_HDA_PPCTL_PIE		BIT(31)
#define SOF_HDA_PPCTL_GPROCEN		BIT(30)

/*Vendor Specific Registers*/
#define SOF_HDA_VS_D0I3C		0x104A

/* D0I3C Register fields */
#define SOF_HDA_VS_D0I3C_CIP		BIT(0) /* Command-In-Progress */
#define SOF_HDA_VS_D0I3C_I3		BIT(2) /* D0i3 enable bit */

/* DPIB entry size: 8 Bytes = 2 DWords */
#define SOF_HDA_DPIB_ENTRY_SIZE	0x8

#define SOF_HDA_SPIB_CAP_ID		0x4
#define SOF_HDA_DRSM_CAP_ID		0x5

#define SOF_HDA_SPIB_BASE		0x08
#define SOF_HDA_SPIB_INTERVAL		0x08
#define SOF_HDA_SPIB_SPIB		0x00
#define SOF_HDA_SPIB_MAXFIFO		0x04

#define SOF_HDA_PPHC_BASE		0x10
#define SOF_HDA_PPHC_INTERVAL		0x10

#define SOF_HDA_PPLC_BASE		0x10
#define SOF_HDA_PPLC_MULTI		0x10
#define SOF_HDA_PPLC_INTERVAL		0x10

#define SOF_HDA_DRSM_BASE		0x08
#define SOF_HDA_DRSM_INTERVAL		0x08

/* Descriptor error interrupt */
#define SOF_HDA_CL_DMA_SD_INT_DESC_ERR		0x10

/* FIFO error interrupt */
#define SOF_HDA_CL_DMA_SD_INT_FIFO_ERR		0x08

/* Buffer completion interrupt */
#define SOF_HDA_CL_DMA_SD_INT_COMPLETE		0x04

#define SOF_HDA_CL_DMA_SD_INT_MASK \
	(SOF_HDA_CL_DMA_SD_INT_DESC_ERR | \
	SOF_HDA_CL_DMA_SD_INT_FIFO_ERR | \
	SOF_HDA_CL_DMA_SD_INT_COMPLETE)
#define SOF_HDA_SD_CTL_DMA_START		0x02 /* Stream DMA start bit */

/* Intel HD Audio Code Loader DMA Registers */
#define SOF_HDA_ADSP_LOADER_BASE		0x80
#define SOF_HDA_ADSP_DPLBASE			0x70
#define SOF_HDA_ADSP_DPUBASE			0x74
#define SOF_HDA_ADSP_DPLBASE_ENABLE		0x01

/* Stream Registers */
#define SOF_HDA_ADSP_REG_CL_SD_CTL		0x00
#define SOF_HDA_ADSP_REG_CL_SD_STS		0x03
#define SOF_HDA_ADSP_REG_CL_SD_LPIB		0x04
#define SOF_HDA_ADSP_REG_CL_SD_CBL		0x08
#define SOF_HDA_ADSP_REG_CL_SD_LVI		0x0C
#define SOF_HDA_ADSP_REG_CL_SD_FIFOW		0x0E
#define SOF_HDA_ADSP_REG_CL_SD_FIFOSIZE		0x10
#define SOF_HDA_ADSP_REG_CL_SD_FORMAT		0x12
#define SOF_HDA_ADSP_REG_CL_SD_FIFOL		0x14
#define SOF_HDA_ADSP_REG_CL_SD_BDLPL		0x18
#define SOF_HDA_ADSP_REG_CL_SD_BDLPU		0x1C
#define SOF_HDA_ADSP_SD_ENTRY_SIZE		0x20

/* CL: Software Position Based FIFO Capability Registers */
#define SOF_DSP_REG_CL_SPBFIFO \
	(SOF_HDA_ADSP_LOADER_BASE + 0x20)
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCH	0x0
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCCTL	0x4
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPIB	0x8
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_MAXFIFOS	0xc

/* Stream Number */
#define SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT	20
#define SOF_HDA_CL_SD_CTL_STREAM_TAG_MASK \
	GENMASK(SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT + 3,\
		SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT)

#define HDA_DSP_HDA_BAR				0
#define HDA_DSP_PP_BAR				1
#define HDA_DSP_SPIB_BAR			2
#define HDA_DSP_DRSM_BAR			3
#define HDA_DSP_BAR				4

#define SRAM_WINDOW_OFFSET(x)			(0x80000 + (x) * 0x20000)

#define HDA_DSP_MBOX_OFFSET			SRAM_WINDOW_OFFSET(0)

#define HDA_DSP_PANIC_OFFSET(x) \
	(((x) & 0xFFFFFF) + HDA_DSP_MBOX_OFFSET)

/* SRAM window 0 FW "registers" */
#define HDA_DSP_SRAM_REG_ROM_STATUS		(HDA_DSP_MBOX_OFFSET + 0x0)
#define HDA_DSP_SRAM_REG_ROM_ERROR		(HDA_DSP_MBOX_OFFSET + 0x4)
/* FW and ROM share offset 4 */
#define HDA_DSP_SRAM_REG_FW_STATUS		(HDA_DSP_MBOX_OFFSET + 0x4)
#define HDA_DSP_SRAM_REG_FW_TRACEP		(HDA_DSP_MBOX_OFFSET + 0x8)
#define HDA_DSP_SRAM_REG_FW_END			(HDA_DSP_MBOX_OFFSET + 0xc)

#define HDA_DSP_MBOX_UPLINK_OFFSET		0x81000

#define HDA_DSP_STREAM_RESET_TIMEOUT		300
/*
 * Timeout in us, for setting the stream RUN bit, during
 * start/stop the stream. The timeout expires if new RUN bit
 * value cannot be read back within the specified time.
 */
#define HDA_DSP_STREAM_RUN_TIMEOUT		300
#define HDA_DSP_CL_TRIGGER_TIMEOUT		300

#define HDA_DSP_SPIB_ENABLE			1
#define HDA_DSP_SPIB_DISABLE			0

#define SOF_HDA_MAX_BUFFER_SIZE			(32 * PAGE_SIZE)

#define HDA_DSP_STACK_DUMP_SIZE			32

/* ROM  status/error values */
#define HDA_DSP_ROM_STS_MASK			GENMASK(23, 0)
#define HDA_DSP_ROM_INIT			0x1
#define HDA_DSP_ROM_FW_MANIFEST_LOADED		0x3
#define HDA_DSP_ROM_FW_FW_LOADED		0x4
#define HDA_DSP_ROM_FW_ENTERED			0x5
#define HDA_DSP_ROM_RFW_START			0xf
#define HDA_DSP_ROM_CSE_ERROR			40
#define HDA_DSP_ROM_CSE_WRONG_RESPONSE		41
#define HDA_DSP_ROM_IMR_TO_SMALL		42
#define HDA_DSP_ROM_BASE_FW_NOT_FOUND		43
#define HDA_DSP_ROM_CSE_VALIDATION_FAILED	44
#define HDA_DSP_ROM_IPC_FATAL_ERROR		45
#define HDA_DSP_ROM_L2_CACHE_ERROR		46
#define HDA_DSP_ROM_LOAD_OFFSET_TO_SMALL	47
#define HDA_DSP_ROM_API_PTR_INVALID		50
#define HDA_DSP_ROM_BASEFW_INCOMPAT		51
#define HDA_DSP_ROM_UNHANDLED_INTERRUPT		0xBEE00000
#define HDA_DSP_ROM_MEMORY_HOLE_ECC		0xECC00000
#define HDA_DSP_ROM_KERNEL_EXCEPTION		0xCAFE0000
#define HDA_DSP_ROM_USER_EXCEPTION		0xBEEF0000
#define HDA_DSP_ROM_UNEXPECTED_RESET		0xDECAF000
#define HDA_DSP_ROM_NULL_FW_ENTRY		0x4c4c4e55
#define HDA_DSP_IPC_PURGE_FW			0x01004000

/* various timeout values */
#define HDA_DSP_PU_TIMEOUT		50
#define HDA_DSP_PD_TIMEOUT		50
#define HDA_DSP_RESET_TIMEOUT_US	50000
#define HDA_DSP_BASEFW_TIMEOUT_US       3000000
#define HDA_DSP_INIT_TIMEOUT_US	500000
#define HDA_DSP_CTRL_RESET_TIMEOUT		100
#define HDA_DSP_WAIT_TIMEOUT		500	/* 500 msec */
#define HDA_DSP_REG_POLL_INTERVAL_US		500	/* 0.5 msec */
#define HDA_DSP_REG_POLL_RETRY_COUNT		50

#define HDA_DSP_ADSPIC_IPC			1
#define HDA_DSP_ADSPIS_IPC			1

/* Intel HD Audio General DSP Registers */
#define HDA_DSP_GEN_BASE		0x0
#define HDA_DSP_REG_ADSPCS		(HDA_DSP_GEN_BASE + 0x04)
#define HDA_DSP_REG_ADSPIC		(HDA_DSP_GEN_BASE + 0x08)
#define HDA_DSP_REG_ADSPIS		(HDA_DSP_GEN_BASE + 0x0C)
#define HDA_DSP_REG_ADSPIC2		(HDA_DSP_GEN_BASE + 0x10)
#define HDA_DSP_REG_ADSPIS2		(HDA_DSP_GEN_BASE + 0x14)

/* Intel HD Audio Inter-Processor Communication Registers */
#define HDA_DSP_IPC_BASE		0x40
#define HDA_DSP_REG_HIPCT		(HDA_DSP_IPC_BASE + 0x00)
#define HDA_DSP_REG_HIPCTE		(HDA_DSP_IPC_BASE + 0x04)
#define HDA_DSP_REG_HIPCI		(HDA_DSP_IPC_BASE + 0x08)
#define HDA_DSP_REG_HIPCIE		(HDA_DSP_IPC_BASE + 0x0C)
#define HDA_DSP_REG_HIPCCTL		(HDA_DSP_IPC_BASE + 0x10)

/* Intel Vendor Specific Registers */
#define HDA_VS_INTEL_EM2		0x1030
#define HDA_VS_INTEL_EM2_L1SEN		BIT(13)

/*  HIPCI */
#define HDA_DSP_REG_HIPCI_BUSY		BIT(31)
#define HDA_DSP_REG_HIPCI_MSG_MASK	0x7FFFFFFF

/* HIPCIE */
#define HDA_DSP_REG_HIPCIE_DONE	BIT(30)
#define HDA_DSP_REG_HIPCIE_MSG_MASK	0x3FFFFFFF

/* HIPCCTL */
#define HDA_DSP_REG_HIPCCTL_DONE	BIT(1)
#define HDA_DSP_REG_HIPCCTL_BUSY	BIT(0)

/* HIPCT */
#define HDA_DSP_REG_HIPCT_BUSY		BIT(31)
#define HDA_DSP_REG_HIPCT_MSG_MASK	0x7FFFFFFF

/* HIPCTE */
#define HDA_DSP_REG_HIPCTE_MSG_MASK	0x3FFFFFFF

#define HDA_DSP_ADSPIC_CL_DMA		0x2
#define HDA_DSP_ADSPIS_CL_DMA		0x2

/* Delay before scheduling D0i3 entry */
#define BXT_D0I3_DELAY 5000

#define FW_CL_STREAM_NUMBER		0x1

/* ADSPCS - Audio DSP Control & Status */

/*
 * Core Reset - asserted high
 * CRST Mask for a given core mask pattern, cm
 */
#define HDA_DSP_ADSPCS_CRST_SHIFT	0
#define HDA_DSP_ADSPCS_CRST_MASK(cm)	((cm) << HDA_DSP_ADSPCS_CRST_SHIFT)

/*
 * Core run/stall - when set to '1' core is stalled
 * CSTALL Mask for a given core mask pattern, cm
 */
#define HDA_DSP_ADSPCS_CSTALL_SHIFT	8
#define HDA_DSP_ADSPCS_CSTALL_MASK(cm)	((cm) << HDA_DSP_ADSPCS_CSTALL_SHIFT)

/*
 * Set Power Active - when set to '1' turn cores on
 * SPA Mask for a given core mask pattern, cm
 */
#define HDA_DSP_ADSPCS_SPA_SHIFT	16
#define HDA_DSP_ADSPCS_SPA_MASK(cm)	((cm) << HDA_DSP_ADSPCS_SPA_SHIFT)

/*
 * Current Power Active - power status of cores, set by hardware
 * CPA Mask for a given core mask pattern, cm
 */
#define HDA_DSP_ADSPCS_CPA_SHIFT	24
#define HDA_DSP_ADSPCS_CPA_MASK(cm)	((cm) << HDA_DSP_ADSPCS_CPA_SHIFT)

/* Mask for a given core index, c = 0.. number of supported cores - 1 */
#define HDA_DSP_CORE_MASK(c)		BIT(c)

/*
 * Mask for a given number of cores
 * nc = number of supported cores
 */
#define SOF_DSP_CORES_MASK(nc)	GENMASK(((nc) - 1), 0)

/* Intel HD Audio Inter-Processor Communication Registers for Cannonlake*/
#define CNL_DSP_IPC_BASE		0xc0
#define CNL_DSP_REG_HIPCTDR		(CNL_DSP_IPC_BASE + 0x00)
#define CNL_DSP_REG_HIPCTDA		(CNL_DSP_IPC_BASE + 0x04)
#define CNL_DSP_REG_HIPCTDD		(CNL_DSP_IPC_BASE + 0x08)
#define CNL_DSP_REG_HIPCIDR		(CNL_DSP_IPC_BASE + 0x10)
#define CNL_DSP_REG_HIPCIDA		(CNL_DSP_IPC_BASE + 0x14)
#define CNL_DSP_REG_HIPCIDD		(CNL_DSP_IPC_BASE + 0x18)
#define CNL_DSP_REG_HIPCCTL		(CNL_DSP_IPC_BASE + 0x28)

/*  HIPCI */
#define CNL_DSP_REG_HIPCIDR_BUSY		BIT(31)
#define CNL_DSP_REG_HIPCIDR_MSG_MASK	0x7FFFFFFF

/* HIPCIE */
#define CNL_DSP_REG_HIPCIDA_DONE	BIT(31)
#define CNL_DSP_REG_HIPCIDA_MSG_MASK	0x7FFFFFFF

/* HIPCCTL */
#define CNL_DSP_REG_HIPCCTL_DONE	BIT(1)
#define CNL_DSP_REG_HIPCCTL_BUSY	BIT(0)

/* HIPCT */
#define CNL_DSP_REG_HIPCTDR_BUSY		BIT(31)
#define CNL_DSP_REG_HIPCTDR_MSG_MASK	0x7FFFFFFF

/* HIPCTDA */
#define CNL_DSP_REG_HIPCTDA_DONE	BIT(31)
#define CNL_DSP_REG_HIPCTDA_MSG_MASK	0x7FFFFFFF

/* HIPCTDD */
#define CNL_DSP_REG_HIPCTDD_MSG_MASK	0x7FFFFFFF

/* BDL */
#define HDA_DSP_BDL_SIZE			4096
#define HDA_DSP_MAX_BDL_ENTRIES			\
	(HDA_DSP_BDL_SIZE / sizeof(struct sof_intel_dsp_bdl))

/* Number of DAIs */
//#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA)
//#define SOF_SKL_NUM_DAIS		14
//#else
#define SOF_SKL_NUM_DAIS		14//8
//#endif

/* Intel HD Audio SRAM Window 0*/
#define HDA_ADSP_SRAM0_BASE_SKL		0x8000

/* Firmware status window */
#define HDA_ADSP_FW_STATUS_SKL		HDA_ADSP_SRAM0_BASE_SKL
#define HDA_ADSP_ERROR_CODE_SKL		(HDA_ADSP_FW_STATUS_SKL + 0x4)

/* Host Device Memory Space */
#define APL_SSP_BASE_OFFSET	0x2000
#define CNL_SSP_BASE_OFFSET	0x10000

/* Host Device Memory Size of a Single SSP */
#define SSP_DEV_MEM_SIZE	0x1000

/* SSP Count of the Platform */
#define APL_SSP_COUNT		6
#define CNL_SSP_COUNT		3
#define ICL_SSP_COUNT		6

/* SSP Registers */
#define SSP_SSC1_OFFSET		0x4
#define SSP_SET_SCLK_SLAVE	BIT(25)
#define SSP_SET_SFRM_SLAVE	BIT(24)
#define SSP_SET_SLAVE		(SSP_SET_SCLK_SLAVE | SSP_SET_SFRM_SLAVE)

#define HDA_IDISP_CODEC(x) ((x) & BIT(2))

struct sof_intel_dsp_bdl {
	__le32 addr_l;
	__le32 addr_h;
	__le32 size;
	__le32 ioc;
} __attribute((packed));

#define SOF_HDA_PLAYBACK_STREAMS	16
#define SOF_HDA_CAPTURE_STREAMS		16
#define SOF_HDA_PLAYBACK		0
#define SOF_HDA_CAPTURE			1

#define SOF_STREAM_SD_OFFSET(s) \
	(SOF_HDA_ADSP_SD_ENTRY_SIZE * ((s)->index) \
	 + SOF_HDA_ADSP_LOADER_BASE)

/* Parameters used to convert the timespec values: */
#define MSEC_PER_SEC	1000L
#define USEC_PER_MSEC	1000L
#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L
#define USEC_PER_SEC	1000000L
#define NSEC_PER_SEC	1000000000L
#define FSEC_PER_SEC	1000000000000000LL

#define SNDRV_PCM_TRIGGER_STOP		0
#define SNDRV_PCM_TRIGGER_START		1
#define SNDRV_PCM_TRIGGER_PAUSE_PUSH	3
#define SNDRV_PCM_TRIGGER_PAUSE_RELEASE	4
#define SNDRV_PCM_TRIGGER_SUSPEND	5
#define SNDRV_PCM_TRIGGER_RESUME	6
#define SNDRV_PCM_TRIGGER_DRAIN		7


#endif