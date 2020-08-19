#ifndef __COMMON_H
#define __COMMON_H
#include <stdbool.h>
#include <linux/const.h>
#include "constants.h"

struct dev {
	int container;
	int group;
	int device;
};

struct snd_dma_buffer {
	struct dev dev;	/* device type */
	unsigned char *area;	/* virtual pointer */
	dma_addr_t addr;	/* physical address */
	size_t bytes;		/* buffer size in bytes */
	void *private_data;	/* private for allocator; don't touch */
};

struct firmware {
	size_t size;
	const __u8 * data;
};

void snd_sof_dsp_write(struct dev * info, __u32 bar, __u32 offset, __u32 value);
__u32 snd_sof_dsp_read(struct dev * info, __u32 bar, __u32 offset);
bool snd_sof_dsp_update_bits_unlocked(struct dev * info, __u32 bar, __u32 offset, __u32 mask, __u32 value);
bool snd_sof_dsp_update_bits(struct dev * info, __u32 bar, __u32 offset,
			     __u32 mask, __u32 value);

void vfio_setup(struct dev * info);
void get_firmware(struct firmware * fw);

int hda_dsp_ctrl_get_caps(struct dev * info);



#endif