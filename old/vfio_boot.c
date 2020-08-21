#include <linux/vfio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "probe.h"

int main() {
	int ret = -100;
	struct dev * info = (struct dev *) malloc(sizeof(struct dev));
	struct firmware * fw = (struct firmware *) malloc(sizeof(struct firmware));
	
	info->container = -1;
	info->group = -1;
	info->device = -1;

	vfio_setup(info);

    load_fw_for_dma(info->container, "/lib/firmware/intel/sof/sof-cnl.ri", 0xfff40000);

	//Start booting process.
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
					1 << 0x7,
					1 << 0x7);

	int sd_offset = SOF_STREAM_SD_OFFSET(0x7);

	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
				sd_offset,
				SOF_HDA_SD_CTL_DMA_START |
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_SD_CTL_DMA_START |
				SOF_HDA_CL_DMA_SD_INT_MASK);


	printf("error: Error code=0x%x: FW status=0x%x\n",
			snd_sof_dsp_read(info, HDA_DSP_BAR,
					 HDA_DSP_SRAM_REG_ROM_ERROR),
			snd_sof_dsp_read(info, HDA_DSP_BAR,
					 HDA_DSP_SRAM_REG_ROM_STATUS));

	__u32 status = snd_sof_dsp_read(info, HDA_DSP_BAR,
					HDA_DSP_SRAM_REG_ROM_STATUS);

	int count = 0;
	sleep(2);
	while(((status & HDA_DSP_ROM_STS_MASK) != HDA_DSP_ROM_FW_ENTERED) & count < 10000){
		status = snd_sof_dsp_read(info, HDA_DSP_BAR,
					HDA_DSP_SRAM_REG_ROM_STATUS);	
		count++;
	}
	

	if((status & HDA_DSP_ROM_STS_MASK) == HDA_DSP_ROM_FW_ENTERED){
		printf("GOod!\n");
	}
	else{
		printf("Bad!\n");
	}

	hda_dsp_ctrl_get_caps(info);



	return 0;
}

// void get_firmware(struct firmware * fw){
// 	int ret = -100;
// 	const char * fw_file = "/lib/firmware/intel/sof/sof-cnl.ri";
//     int f = open(fw_file, O_RDWR);
//     FILE * fp;
//     fp = fopen(fw_file, "r");
//     fseek(fp, 0L, SEEK_END);
//     long actual_size = ftell(fp);
// 	__u8 * data = (__u8 *) malloc(actual_size);
// 	if(data == NULL){
// 		printf("malloc of %d bytes failed", actual_size);
// 	}
//     rewind(fp);
//     fclose(fp);
//     printf("\n\nActual size: %ld 0x%x\n\n", actual_size, actual_size);
//     if(f < 0){
//         printf("opening firmware file failed\n");
//     }
//     else{
//         ret = read(f, data, actual_size);
//         if(ret < 0){
//             printf("Read firmware failed\n");
//         }
//     }
	
// 	fw->data = data;
// 	fw->size = actual_size;
// }


void vfio_setup(struct dev * info){
	int i = 0;

	int ret = -100;

	struct vfio_group_status group_status = {.argsz = sizeof(group_status)};
	
	struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

	/* Create a new container */
	info->container = open("/dev/vfio/vfio", O_RDWR);
	if(info->container < 0){
		printf("Container did not open properly\n");
	}
	else{
		printf("Container fd: %d\n", info->container);
	}

	if (ioctl(info->container, VFIO_GET_API_VERSION) != VFIO_API_VERSION){
		/* Unknown API version */
		printf("Unknown API version\n");
	}

	if (!ioctl(info->container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)){
		/* Doesn't support the IOMMU driver we want. */
		printf("Wrong IOMMU version\n");
	}

	/* Open the group */
	info->group = open("/dev/vfio/11", O_RDWR);
	if(info->group < 0){
		printf("Group didnt open correctly\n");
	}
	else{
		printf("Groupd fd: %d\n", info->group);
	}
	/* Test the group is viable and available */
	ret = ioctl(info->group, VFIO_GROUP_GET_STATUS, &group_status);
	if(ret < 0){
		printf("getting group status failed. Error: %d\n", ret);
	}
	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)){
		/* Group is not viable (ie, not all devices bound for vfio) */
		printf("Not all devices in group are bound vfio\n");
	}
	/* Add the group to the container */
	ret = ioctl(info->group, VFIO_GROUP_SET_CONTAINER, &info->container);
	if(ret < 0){
		printf("adding group to container failed. Error: %d\n", ret);
	}
	/* Enable the IOMMU model we want */
	ret = ioctl(info->container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if(ret < 0){
		printf("Setting IOMMU type failed. Error: %d\n", ret);
	}
	iommu_info.iova_pgsizes = 1324u;
	/* Get addition IOMMU info */
	ret = ioctl(info->container, VFIO_IOMMU_GET_INFO, &iommu_info);
	if(ret < 0){
		printf("getting iommu info failed. Error: %d\n", ret);
	}
	printf("flags iommu: %u\n",iommu_info.flags);

	/* Get a file descriptor for the device */
	info->device = ioctl(info->group, VFIO_GROUP_GET_DEVICE_FD, "0000:00:1f.3");
	if(info->device < 0){
		printf("Device FD not found\n");
	}
	else{
		printf("Device FD: %d\n", info->device);
	}

	/* Test and setup the device */
	ret = ioctl(info->device, VFIO_DEVICE_GET_INFO, &device_info);
	if(ret < 0){
		printf("Getting device info failed. error: %d\n", ret);
	}
	printf("Device 3 regions flags, regions, irqs: %u %u %u\n", device_info.flags, device_info.num_regions, device_info.num_irqs);

	for (i = 0; i < device_info.num_regions; i++) {
		struct vfio_region_info reg = { .argsz = sizeof(reg) };

		reg.index = i;

		ret = ioctl(info->device, VFIO_DEVICE_GET_REGION_INFO, &reg);
		if(ret < 0){
			printf("Error getting device region info. %d\n", ret);
		}
		else{
			printf("Index:%d , Flags:%u , Size:%llu , Offset:%llu.\n", i, reg.flags, reg.size, reg.offset);


		}

		

		/* Setup mappings... read/write offsets, mmaps
		 * For PCI devices, config space is a region */
	}
	printf("\n\n");
}


int hda_dsp_ctrl_get_caps(struct dev * info)
{
	//struct hdac_bus *bus = sof_to_bus(info);
	__u32 cap, offset, feature;
	int count = 0;

	offset = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR, SOF_HDA_LLCH);

	do {
		cap = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR, offset);

		feature = (cap & SOF_HDA_CAP_ID_MASK) >> SOF_HDA_CAP_ID_OFF;

		switch (feature) {
		case SOF_HDA_PP_CAP_ID:
			printf("found DSP capability at 0x%x\n",
				offset);
			break;
		case SOF_HDA_SPIB_CAP_ID:
			printf("found SPIB capability at 0x%x\n",
				offset);
			break;
		case SOF_HDA_DRSM_CAP_ID:
			printf("found DRSM capability at 0x%x\n",
				offset);
			break;
		case SOF_HDA_GTS_CAP_ID:
			printf("found GTS capability at 0x%x\n",
				offset);
			break;
		case SOF_HDA_ML_CAP_ID:
			printf("found ML capability at 0x%x\n",
				offset);
			break;
		default:
			printf("found capability %d at 0x%x\n",
				 feature, offset);
			break;
		}

		offset = cap & SOF_HDA_CAP_NEXT_MASK;
	} while (count++ <= SOF_HDA_MAX_CAPS && offset);

	return 0;
}



// int hda_dsp_stream_hw_params(struct dev *info, struct firmware * fw,
// 			     struct snd_pcm_hw_params *params)
// {
// 	__u32 fw_addr = (__u32) fw->data;
// 	int sd_offset = SOF_STREAM_SD_OFFSET(0x7);
// 	int ret, timeout = HDA_DSP_STREAM_RESET_TIMEOUT;
// 	__u32 dma_start = SOF_HDA_SD_CTL_DMA_START;
// 	__u32 val, mask;
// 	__u32 run;

// 	/* decouple host and link DMA */
// 	mask = 0x1 << 0x7;
// 	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
// 				mask, mask);

// 	if (!fw->data) {
// 		printf("error: no dma buffer allocated!\n");
// 		return -ENODEV;
// 	}

// 	/* clear stream status */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
// 				SOF_HDA_CL_DMA_SD_INT_MASK |
// 				SOF_HDA_SD_CTL_DMA_START, 0);

// 	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_HDA_BAR,
// 					    sd_offset, run,
// 					    !(run & dma_start),
// 					    HDA_DSP_REG_POLL_INTERVAL_US,
// 					    HDA_DSP_STREAM_RUN_TIMEOUT);

// 	if (ret < 0) {
// 		printf("error: %s: timeout on STREAM_SD_OFFSET read1\n",
// 			__func__);
// 		return ret;
// 	}

// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
// 				sd_offset + SOF_HDA_ADSP_REG_CL_SD_STS,
// 				SOF_HDA_CL_DMA_SD_INT_MASK,
// 				SOF_HDA_CL_DMA_SD_INT_MASK);

// 	/* stream reset */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset, 0x1,
// 				0x1);
// 	usleep(3);
// 	do {
// 		val = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR,
// 				       sd_offset);
// 		if (val & 0x1)
// 			break;
// 	} while (--timeout);
// 	if (timeout == 0) {
// 		printf("error: stream reset failed\n");
// 		return -ETIMEDOUT;
// 	}

// 	timeout = HDA_DSP_STREAM_RESET_TIMEOUT;
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset, 0x1,
// 				0x0);

// 	/* wait for hardware to report that stream is out of reset */
// 	usleep(3);
// 	do {
// 		val = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR,
// 				       sd_offset);
// 		if ((val & 0x1) == 0)
// 			break;
// 	} while (--timeout);
// 	if (timeout == 0) {
// 		printf("error: timeout waiting for stream reset\n");
// 		return -ETIMEDOUT;
// 	}

// 	if (hstream->posbuf)
// 		*hstream->posbuf = 0;

// 	/* reset BDL address */
// 	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
// 			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPL,
// 			  0x0);
// 	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
// 			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPU,
// 			  0x0);

// 	/* clear stream status */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
// 				SOF_HDA_CL_DMA_SD_INT_MASK |
// 				SOF_HDA_SD_CTL_DMA_START, 0);

// 	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_HDA_BAR,
// 					    sd_offset, run,
// 					    !(run & dma_start),
// 					    HDA_DSP_REG_POLL_INTERVAL_US,
// 					    HDA_DSP_STREAM_RUN_TIMEOUT);

// 	if (ret < 0) {
// 		printf("error: %s: timeout on STREAM_SD_OFFSET read2\n",
// 			__func__);
// 		return ret;
// 	}

// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
// 				sd_offset + SOF_HDA_ADSP_REG_CL_SD_STS,
// 				SOF_HDA_CL_DMA_SD_INT_MASK,
// 				SOF_HDA_CL_DMA_SD_INT_MASK);

// 	hstream->frags = 0;

// 	ret = hda_dsp_stream_setup_bdl(sdev, dmab, hstream);
// 	if (ret < 0) {
// 		dev_err(sdev->dev, "error: set up of BDL failed\n");
// 		return ret;
// 	}

// 	/* program stream tag to set up stream descriptor for DMA */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
// 				SOF_HDA_CL_SD_CTL_STREAM_TAG_MASK,
// 				0x7 <<
// 				SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT);

// 	/* program cyclic buffer length */
// 	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
// 			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_CBL,
// 			  hstream->bufsize);

// 	/*
// 	 * Recommended hardware programming sequence for HDAudio DMA format
// 	 *
// 	 * 1. Put DMA into coupled mode by clearing PPCTL.PROCEN bit
// 	 *    for corresponding stream index before the time of writing
// 	 *    format to SDxFMT register.
// 	 * 2. Write SDxFMT
// 	 * 3. Set PPCTL.PROCEN bit for corresponding stream index to
// 	 *    enable decoupled mode
// 	 */

// 	/* couple host and link DMA, disable DSP features */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
// 				mask, 0);

// 	/* program stream format */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
// 				sd_offset +
// 				SOF_HDA_ADSP_REG_CL_SD_FORMAT,
// 				0xffff, hstream->format_val);

// 	/* decouple host and link DMA, enable DSP features */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
// 				mask, mask);

// 	/* program last valid index */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
// 				sd_offset + SOF_HDA_ADSP_REG_CL_SD_LVI,
// 				0xffff, (hstream->frags - 1));

// 	/* program BDL address */
// 	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
// 			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPL,
// 			  (__u32)fw_addr);
// 	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
// 			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPU,
// 			  upper_32_bits(fw_addr));

// 	/* enable position buffer */
// 	if (!(snd_sof_dsp_read(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE)
// 				& SOF_HDA_ADSP_DPLBASE_ENABLE)) {
// 		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPUBASE,
// 				  upper_32_bits(bus->posbuf.addr));
// 		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE,
// 				  (__u32)bus->posbuf.addr |
// 				  SOF_HDA_ADSP_DPLBASE_ENABLE);
// 	}

// 	/* set interrupt enable bits */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
// 				SOF_HDA_CL_DMA_SD_INT_MASK,
// 				SOF_HDA_CL_DMA_SD_INT_MASK);

// 	/* read FIFO size */
// 	if (hstream->direction == SNDRV_PCM_STREAM_PLAYBACK) {
// 		hstream->fifo_size =
// 			snd_sof_dsp_read(info, HDA_DSP_HDA_BAR,
// 					 sd_offset +
// 					 SOF_HDA_ADSP_REG_CL_SD_FIFOSIZE);
// 		hstream->fifo_size &= 0xffff;
// 		hstream->fifo_size += 1;
// 	} else {
// 		hstream->fifo_size = 0;
// 	}

// 	return ret;
// }

__u32 snd_sof_dsp_read(struct dev * info, __u32 bar, __u32 offset){
	int device = info->device;
	int ret;

	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	reg.index = bar;

	ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if(ret < 0){
		printf("Error getting device region. %d\n", ret);
	}
	else{
		__u32 result;
		ret = pread(device, &result, sizeof(__u32), reg.offset+offset);
		if(ret < 0){
			printf("read err in dsp_read %d\n", ret);
		}
		else{
			return result;		
		}
	}
	printf("read Didnt work\n");
	return 0;
	
}

void snd_sof_dsp_write(struct dev * info, __u32 bar, __u32 offset, __u32 value){
	int device = info->device;
	int ret;

	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	reg.index = bar;

	ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if(ret < 0){
		printf("Error getting device region. %d\n", ret);
		return;
	}
	else{
		ret = pwrite(device, &value, sizeof(value), reg.offset+offset);
		if(ret < 0){
			printf("write err in dsp_write %d\n", ret);
			return;
		}
	}
	printf("write worked\n");
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


// int hda_dsp_core_power_up(struct dev* info, unsigned int core_mask)
// {
// 	unsigned int cpa;
// 	__u32 adspcs;
// 	int ret;

// 	/* update bits */
// 	snd_sof_dsp_update_bits(info, HDA_DSP_BAR, HDA_DSP_REG_ADSPCS,
// 				HDA_DSP_ADSPCS_SPA_MASK(core_mask),
// 				HDA_DSP_ADSPCS_SPA_MASK(core_mask));

// 	/* poll with timeout to check if operation successful */
// 	cpa = HDA_DSP_ADSPCS_CPA_MASK(core_mask);
// 	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
// 					    HDA_DSP_REG_ADSPCS, adspcs,
// 					    (adspcs & cpa) == cpa,
// 					    HDA_DSP_REG_POLL_INTERVAL_US,
// 					    HDA_DSP_RESET_TIMEOUT_US);
// 	if (ret < 0) {
// 		dev_err(sdev->dev,
// 			"error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
// 			__func__);
// 		return ret;
// 	}

// 	/* did core power up ? */
// 	adspcs = snd_sof_dsp_read(info, HDA_DSP_BAR,
// 				  HDA_DSP_REG_ADSPCS);
// 	if ((adspcs & HDA_DSP_ADSPCS_CPA_MASK(core_mask)) !=
// 		HDA_DSP_ADSPCS_CPA_MASK(core_mask)) {
// 		dev_err(sdev->dev,
// 			"error: power up core failed core_mask %xadspcs 0x%x\n",
// 			core_mask, adspcs);
// 		ret = -EIO;
// 	}

// 	return ret;
// }