#include <linux/vfio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "constants.h"
#include "sof-vfio.h"

__u32 snd_sof_dsp_read(struct dev * info, __u32 bar, __u32 offset){
	int device = info->device;
	int ret;
	__u32 result;
	ret = pread(device, &result, sizeof(__u32), info->offsets[bar]+offset);
	if(ret < 0){
		printf("read err in dsp_read %d\n", ret);
	}
	else{
		return result;		
	}
	printf("read Didnt work\n");
	return 0;
	
}

void snd_sof_dsp_write(struct dev * info, __u32 bar, __u32 offset, __u32 value){
	int device = info->device;
	int ret;

	ret = pwrite(device, &value, sizeof(value), info->offsets[bar]+offset);
	if(ret < 0){
		printf("write err in dsp_write %d\n", ret);
		return;
	}
	//printf("write worked\n");

	return;
	
}

void sof_io_write(struct dev * info, unsigned long long offset, unsigned int value){
	int device = info->device;
	int ret;

	ret = pwrite(device, &value, sizeof(value), offset);
	if(ret < 0){
		printf("write err in io_write %d\n", ret);
		return;
	}
	//printf("write worked\n");

	return;	
}


bool snd_sof_dsp_update_bits(struct dev * info, __u32 bar, __u32 offset,
			     __u32 mask, __u32 value)
{

	bool change = snd_sof_dsp_update_bits_unlocked(info, bar, offset, mask,
						  value);
	return change;
}


bool snd_sof_dsp_update_bits_unlocked(struct dev * info, __u32 bar, __u32 offset, __u32 mask, __u32 value)
{
	unsigned int old;
	unsigned int new_val;
	__u32 ret;

	ret = snd_sof_dsp_read(info, bar, offset);

	old = ret;
	new_val = (old & ~mask) | (value & mask);

	if (old == new_val)
		return false;

	snd_sof_dsp_write(info, bar, offset, new_val);

	return true;
}

void snd_sof_dsp_update_bits_forced(struct dev *info, __u32 bar,
				    __u32 offset, __u32 mask, __u32 value)
{
	//unsigned long flags;

	//spin_lock_irqsave(&sdev->hw_lock, flags);
	snd_sof_dsp_update_bits_unlocked(info, bar, offset, mask, value);
	//spin_unlock_irqrestore(&sdev->hw_lock, flags);
}


bool snd_sof_pci_update_bits(struct dev *info, __u32 offset,
			     __u32 mask, __u32 value)
{
	bool change;

	change = snd_sof_dsp_update_bits_unlocked(info, VFIO_PCI_CONFIG_REGION_INDEX, offset, mask, value);
	return change;
}



const struct sof_intel_dsp_desc * get_chip_info(){
    struct sof_intel_dsp_desc * cnl_chip_info = (struct sof_intel_dsp_desc *) malloc(sizeof(struct sof_intel_dsp_desc));
	/* Cannonlake */
	cnl_chip_info->cores_num = 4;
	cnl_chip_info->init_core_mask = 1;
	cnl_chip_info->cores_mask = HDA_DSP_CORE_MASK(0) |
				HDA_DSP_CORE_MASK(1) |
				HDA_DSP_CORE_MASK(2) |
				HDA_DSP_CORE_MASK(3);
	cnl_chip_info->ipc_req = CNL_DSP_REG_HIPCIDR;
	cnl_chip_info->ipc_req_mask = CNL_DSP_REG_HIPCIDR_BUSY;
	cnl_chip_info->ipc_ack = CNL_DSP_REG_HIPCIDA;
	cnl_chip_info->ipc_ack_mask = CNL_DSP_REG_HIPCIDA_DONE;
	cnl_chip_info->ipc_ctl = CNL_DSP_REG_HIPCCTL;
	cnl_chip_info->rom_init_timeout	= 300;
	cnl_chip_info->ssp_count = CNL_SSP_COUNT;
	cnl_chip_info->ssp_base_offset = CNL_SSP_BASE_OFFSET;

    return cnl_chip_info;
}

