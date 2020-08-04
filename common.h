#ifndef __COMMON_H
#define __COMMON_H
#include <stdbool.h>
#include <linux/const.h>

#ifndef BITS_PER_LONG
#define BITS_PER_LONG 32
#endif

#define _UL(x) _BITUL(x)

#define GENMASK(h, l) \
	(((~_UL(0)) - (_UL(1) << (l)) + 1) & \
	 (~_UL(0) >> (BITS_PER_LONG - 1 - (h))))



#define SND_SOF_FW_SIG_SIZE 4
#define HDA_DSP_BAR 4

#define HDA_DSP_ROM_STS_MASK             GENMASK(23, 0)
#define HDA_DSP_ROM_FW_ENTERED 0x5


#define SRAM_WINDOW_OFFSET(x)			(0x80000 + (x) * 0x20000)
#define HDA_DSP_MBOX_OFFSET			SRAM_WINDOW_OFFSET(0)

#define HDA_DSP_SRAM_REG_ROM_ERROR (HDA_DSP_MBOX_OFFSET + 0x4)
#define HDA_DSP_SRAM_REG_ROM_STATUS (HDA_DSP_MBOX_OFFSET + 0x0)

#define HDA_DSP_HDA_BAR 0
#define SOF_HDA_INTCTL 0x20
#define SOF_HDA_SD_CTL_DMA_START 0x02

#define SOF_HDA_CL_DMA_SD_INT_DESC_ERR 0x10
#define SOF_HDA_CL_DMA_SD_INT_FIFO_ERR 0x08
#define SOF_HDA_CL_DMA_SD_INT_COMPLETE 0x04

#define SOF_HDA_CL_DMA_SD_INT_MASK \
	(SOF_HDA_CL_DMA_SD_INT_DESC_ERR | \
	SOF_HDA_CL_DMA_SD_INT_FIFO_ERR | \
	SOF_HDA_CL_DMA_SD_INT_COMPLETE)


#define SOF_HDA_ADSP_SD_ENTRY_SIZE 0x20
#define SOF_HDA_ADSP_LOADER_BASE 0x80

#define SOF_STREAM_SD_OFFSET(s) \
	(SOF_HDA_ADSP_SD_ENTRY_SIZE * s \
	 + SOF_HDA_ADSP_LOADER_BASE)





typedef struct dev {
	int container;
	int group;
	int device;
} dev;

typedef struct firmware {
	size_t size;
	const __u8 * data;
} firmware;

void snd_sof_dsp_write(dev * info, __u32 bar, __u32 offset, __u32 value);
__u32 snd_sof_dsp_read(dev * info, __u32 bar, __u32 offset);
bool snd_sof_dsp_update_bits_unlocked(dev * info, __u32 bar, __u32 offset, __u32 mask, __u32 value);
bool snd_sof_dsp_update_bits(dev * info, __u32 bar, __u32 offset,
			     __u32 mask, __u32 value);






#endif