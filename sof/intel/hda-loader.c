// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Authors: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//	    Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//	    Rander Wang <rander.wang@intel.com>
//          Keyon Jie <yang.jie@linux.intel.com>
//

/*
 * Hardware interface for HDA DSP code loader
 */

#include <linux/firmware.h>
#include <sound/hdaudio_ext.h>
#include <sound/sof.h>
#include "../ops.h"
#include "hda.h"
#include <linux/vfio.h>


int main() {
	int container = -1, group = -1, device = -1, i = 0;
	struct vfio_group_status group_status = {.argsz = sizeof(group_status)};
	
	struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
	struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

	/* Create a new container */
	container = open("/dev/vfio/vfio", O_RDWR);
	if(container < 0){
		printf("Container did not open properly\n");
	}
	else{
		printf("Container fd: %d\n", container);
	}

	if (ioctl(container, VFIO_GET_API_VERSION) != VFIO_API_VERSION){
		/* Unknown API version */
		printf("Unknown API version\n");
	}

	if (!ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)){
		/* Doesn't support the IOMMU driver we want. */
		printf("Wrong IOMMU version\n");
	}

	/* Open the group */
	group = open("/dev/vfio/11", O_RDWR);
	if(group < 0){
		printf("Group didnt open correctly\n");
	}
	else{
		printf("Groupd fd: %d\n", group);
	}
	int ret = -100;
	/* Test the group is viable and available */
	ret = ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);
	if(ret < 0){
		printf("getting group status failed. Error: %d\n", ret);
	}
	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)){
		/* Group is not viable (ie, not all devices bound for vfio) */
		printf("Not all devices in group are bound vfio\n");
	}
	/* Add the group to the container */
	ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if(ret < 0){
		printf("adding group to container failed. Error: %d\n", ret);
	}
	/* Enable the IOMMU model we want */
	ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if(ret < 0){
		printf("Setting IOMMU type failed. Error: %d\n", ret);
	}
	iommu_info.iova_pgsizes = 1324u;
	/* Get addition IOMMU info */
	ret = ioctl(container, VFIO_IOMMU_GET_INFO, &iommu_info);
	if(ret < 0){
		printf("getting iommu info failed. Error: %d\n", ret);
	}
	printf("flags iommu: %u\n",iommu_info.flags);

	/* Allocate some space and setup a DMA mapping */
	dma_map.vaddr = (__u64) mmap(0, 1024*1024, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	dma_map.size = 1024*1024;
	dma_map.iova = 0; /* 1MB starting at 0x0 from device view */
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
	if(ret < 0){
		printf("IOMMU MAP DMA failed. %d\n", ret);
	}

	printf("DMA Map info. Size: %llu, flags: %u\n", dma_map.size, dma_map.flags);

	/* Get a file descriptor for the device */
	device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, "0000:00:1f.3");
	if(device < 0){
		printf("Device FD not found\n");
	}
	else{
		printf("Device FD: %d\n", device);
	}

	/* Test and setup the device */
	ret = ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);
	if(ret < 0){
		printf("Getting device info failed. error: %d\n", ret);
	}
	printf("Device 3 regions flags, regions, irqs: %u %u %u\n", device_info.flags, device_info.num_regions, device_info.num_irqs);

	
	printf("\n\n");
	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	reg.index = VFIO_PCI_CONFIG_REGION_INDEX;

	ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if(ret < 0){
		printf("Error getting config space device region. %d\n", ret);
	}
	else{
		void * numbers = malloc(reg.size);
		ret = pread(device, numbers, reg.size, reg.offset);
		if(ret < 0){
			printf("read err %d\n", ret);
		}
		else{
			printf("read %d bytes\n", ret);
			unsigned short * add  = (unsigned short *) numbers;
			for(i = 0; i < reg.size/8; i+=2){
				printf("%08x", i*8);
				for(int j = 0; j < 8; j++){
					printf(" %04x", *add);						
					add++;
				}
				printf("\n");
			}
		}
	}
	printf("\n\n");
	


	return 0;
}


#define HDA_FW_BOOT_ATTEMPTS	3

static int cl_stream_prepare(struct snd_sof_dev *sdev, unsigned int format,
			     unsigned int size, struct snd_dma_buffer *dmab,
			     int direction)
{
	struct hdac_ext_stream *dsp_stream;
	struct hdac_stream *hstream;
	struct pci_dev *pci = to_pci_dev(sdev->dev);
	int ret;

	if (direction != SNDRV_PCM_STREAM_PLAYBACK) {
		dev_err(sdev->dev, "error: code loading DMA is playback only\n");
		return -EINVAL;
	}

	dsp_stream = hda_dsp_stream_get(sdev, direction);

	if (!dsp_stream) {
		dev_err(sdev->dev, "error: no stream available\n");
		return -ENODEV;
	}
	hstream = &dsp_stream->hstream;
	hstream->substream = NULL;

	/* allocate DMA buffer */
	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV_SG, &pci->dev, size, dmab);
	if (ret < 0) {
		dev_err(sdev->dev, "error: memory alloc failed: %x\n", ret);
		goto error;
	}

	hstream->period_bytes = 0;/* initialize period_bytes */
	hstream->format_val = format;
	hstream->bufsize = size;

	ret = hda_dsp_stream_hw_params(sdev, dsp_stream, dmab, NULL);
	if (ret < 0) {
		dev_err(sdev->dev, "error: hdac prepare failed: %x\n", ret);
		goto error;
	}

	hda_dsp_stream_spib_config(sdev, dsp_stream, HDA_DSP_SPIB_ENABLE, size);

	return hstream->stream_tag;

error:
	hda_dsp_stream_put(sdev, direction, hstream->stream_tag);
	snd_dma_free_pages(dmab);
	return ret;
}

/*
 * first boot sequence has some extra steps. core 0 waits for power
 * status on core 1, so power up core 1 also momentarily, keep it in
 * reset/stall and then turn it off
 */
static int cl_dsp_init(struct snd_sof_dev *sdev, const void *fwdata,
		       u32 fwsize, int stream_tag)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;
	unsigned int status;
	int ret;
	int i;

	/* step 1: power up corex */
	ret = hda_dsp_core_power_up(sdev, chip->cores_mask);
	if (ret < 0) {
		dev_err(sdev->dev, "error: dsp core 0/1 power up failed\n");
		goto err;
	}

	/* DSP is powered up, set all SSPs to slave mode */
	for (i = 0; i < chip->ssp_count; i++) {
		snd_sof_dsp_update_bits_unlocked(sdev, HDA_DSP_BAR,
						 chip->ssp_base_offset
						 + i * SSP_DEV_MEM_SIZE
						 + SSP_SSC1_OFFSET,
						 SSP_SET_SLAVE,
						 SSP_SET_SLAVE);
	}

	/* step 2: purge FW request */
	snd_sof_dsp_write(sdev, HDA_DSP_BAR, chip->ipc_req,
			  chip->ipc_req_mask | (HDA_DSP_IPC_PURGE_FW |
			  ((stream_tag - 1) << 9)));

	/* step 3: unset core 0 reset state & unstall/run core 0 */
	ret = hda_dsp_core_run(sdev, HDA_DSP_CORE_MASK(0));
	if (ret < 0) {
		dev_err(sdev->dev, "error: dsp core start failed %d\n", ret);
		ret = -EIO;
		goto err;
	}

	/* step 4: wait for IPC DONE bit from ROM */
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
					    chip->ipc_ack, status,
					    ((status & chip->ipc_ack_mask)
						    == chip->ipc_ack_mask),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_INIT_TIMEOUT_US);

	if (ret < 0) {
		dev_err(sdev->dev, "error: %s: timeout for HIPCIE done\n",
			__func__);
		goto err;
	}

	/* set DONE bit to clear the reply IPC message */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR,
				       chip->ipc_ack,
				       chip->ipc_ack_mask,
				       chip->ipc_ack_mask);

	/* step 5: power down corex */
	ret = hda_dsp_core_power_down(sdev,
				  chip->cores_mask & ~(HDA_DSP_CORE_MASK(0)));
	if (ret < 0) {
		dev_err(sdev->dev, "error: dsp core x power down failed\n");
		goto err;
	}

	/* step 6: enable IPC interrupts */
	hda_dsp_ipc_int_enable(sdev);

	/* step 7: wait for ROM init */
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
					HDA_DSP_SRAM_REG_ROM_STATUS, status,
					((status & HDA_DSP_ROM_STS_MASK)
						== HDA_DSP_ROM_INIT),
					HDA_DSP_REG_POLL_INTERVAL_US,
					chip->rom_init_timeout *
					USEC_PER_MSEC);
	if (!ret)
		return 0;

	dev_err(sdev->dev,
		"error: %s: timeout HDA_DSP_SRAM_REG_ROM_STATUS read\n",
		__func__);

err:
	hda_dsp_dump(sdev, SOF_DBG_REGS | SOF_DBG_PCI | SOF_DBG_MBOX);
	hda_dsp_core_reset_power_down(sdev, chip->cores_mask);

	return ret;
}

static int cl_trigger(struct snd_sof_dev *sdev,
		      struct hdac_ext_stream *stream, int cmd)
{
	struct hdac_stream *hstream = &stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);

	/* code loader is special case that reuses stream ops */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		wait_event_timeout(sdev->waitq, !sdev->code_loading,
				   HDA_DSP_CL_TRIGGER_TIMEOUT);

		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
					1 << hstream->index,
					1 << hstream->index);

		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
					sd_offset,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK);

		hstream->running = true;
		return 0;
	default:
		return hda_dsp_stream_trigger(sdev, stream, cmd);
	}
}

static struct hdac_ext_stream *get_stream_with_tag(struct snd_sof_dev *sdev,
						   int tag)
{
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_stream *s;

	/* get stream with tag */
	list_for_each_entry(s, &bus->stream_list, list) {
		if (s->direction == SNDRV_PCM_STREAM_PLAYBACK &&
		    s->stream_tag == tag) {
			return stream_to_hdac_ext_stream(s);
		}
	}

	return NULL;
}

static int cl_cleanup(struct snd_sof_dev *sdev, struct snd_dma_buffer *dmab,
		      struct hdac_ext_stream *stream)
{
	struct hdac_stream *hstream = &stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	int ret;

	ret = hda_dsp_stream_spib_config(sdev, stream, HDA_DSP_SPIB_DISABLE, 0);

	hda_dsp_stream_put(sdev, SNDRV_PCM_STREAM_PLAYBACK,
			   hstream->stream_tag);
	hstream->running = 0;
	hstream->substream = NULL;

	/* reset BDL address */
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPL, 0);
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPU, 0);

	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR, sd_offset, 0);
	snd_dma_free_pages(dmab);
	dmab->area = NULL;
	hstream->bufsize = 0;
	hstream->format_val = 0;

	return ret;
}

static int cl_copy_fw(struct snd_sof_dev *sdev, struct hdac_ext_stream *stream)
{
	unsigned int reg;
	int ret, status;

	ret = cl_trigger(sdev, stream, SNDRV_PCM_TRIGGER_START);
	if (ret < 0) {
		dev_err(sdev->dev, "error: DMA trigger start failed\n");
		return ret;
	}

	status = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR,
					HDA_DSP_SRAM_REG_ROM_STATUS, reg,
					((reg & HDA_DSP_ROM_STS_MASK)
						== HDA_DSP_ROM_FW_ENTERED),
					HDA_DSP_REG_POLL_INTERVAL_US,
					HDA_DSP_BASEFW_TIMEOUT_US);

	/*
	 * even in case of errors we still need to stop the DMAs,
	 * but we return the initial error should the DMA stop also fail
	 */

	if (status < 0) {
		dev_err(sdev->dev,
			"error: %s: timeout HDA_DSP_SRAM_REG_ROM_STATUS read\n",
			__func__);
	}

	ret = cl_trigger(sdev, stream, SNDRV_PCM_TRIGGER_STOP);
	if (ret < 0) {
		dev_err(sdev->dev, "error: DMA trigger stop failed\n");
		if (!status)
			status = ret;
	}

	return status;
}

int hda_dsp_cl_boot_firmware(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *plat_data = sdev->pdata;
	const struct sof_dev_desc *desc = plat_data->desc;
	const struct sof_intel_dsp_desc *chip_info;
	struct hdac_ext_stream *stream;
	struct firmware stripped_firmware;
	int ret, ret1, tag, i;

	chip_info = desc->chip_info;

	stripped_firmware.data = plat_data->fw->data;
	stripped_firmware.size = plat_data->fw->size;

	/* init for booting wait */
	init_waitqueue_head(&sdev->boot_wait);
	sdev->boot_complete = false;

	/* prepare DMA for code loader stream */
	tag = cl_stream_prepare(sdev, 0x40, stripped_firmware.size,
				&sdev->dmab, SNDRV_PCM_STREAM_PLAYBACK);

	if (tag < 0) {
		dev_err(sdev->dev, "error: dma prepare for fw loading err: %x\n",
			tag);
		return tag;
	}

	/* get stream with tag */
	stream = get_stream_with_tag(sdev, tag);
	if (!stream) {
		dev_err(sdev->dev,
			"error: could not get stream with stream tag %d\n",
			tag);
		ret = -ENODEV;
		goto err;
	}

	memcpy(sdev->dmab.area, stripped_firmware.data,
	       stripped_firmware.size);

	/* try ROM init a few times before giving up */
	for (i = 0; i < HDA_FW_BOOT_ATTEMPTS; i++) {
		ret = cl_dsp_init(sdev, stripped_firmware.data,
				  stripped_firmware.size, tag);

		/* don't retry anymore if successful */
		if (!ret)
			break;

		dev_err(sdev->dev, "error: Error code=0x%x: FW status=0x%x\n",
			snd_sof_dsp_read(sdev, HDA_DSP_BAR,
					 HDA_DSP_SRAM_REG_ROM_ERROR),
			snd_sof_dsp_read(sdev, HDA_DSP_BAR,
					 HDA_DSP_SRAM_REG_ROM_STATUS));
		dev_err(sdev->dev, "error: iteration %d of Core En/ROM load failed: %d\n",
			i, ret);
	}

	if (i == HDA_FW_BOOT_ATTEMPTS) {
		dev_err(sdev->dev, "error: dsp init failed after %d attempts with err: %d\n",
			i, ret);
		goto cleanup;
	}

	/*
	 * at this point DSP ROM has been initialized and
	 * should be ready for code loading and firmware boot
	 */
	ret = cl_copy_fw(sdev, stream);
	if (!ret)
		dev_dbg(sdev->dev, "Firmware download successful, booting...\n");
	else
		dev_err(sdev->dev, "error: load fw failed ret: %d\n", ret);

cleanup:
	/*
	 * Perform codeloader stream cleanup.
	 * This should be done even if firmware loading fails.
	 * If the cleanup also fails, we return the initial error
	 */
	ret1 = cl_cleanup(sdev, &sdev->dmab, stream);
	if (ret1 < 0) {
		dev_err(sdev->dev, "error: Code loader DSP cleanup failed\n");

		/* set return value to indicate cleanup failure */
		if (!ret)
			ret = ret1;
	}

	/*
	 * return master core id if both fw copy
	 * and stream clean up are successful
	 */
	if (!ret)
		return chip_info->init_core_mask;

	/* dump dsp registers and disable DSP upon error */
err:
	hda_dsp_dump(sdev, SOF_DBG_REGS | SOF_DBG_PCI | SOF_DBG_MBOX);

	/* disable DSP */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR,
				SOF_HDA_REG_PP_PPCTL,
				SOF_HDA_PPCTL_GPROCEN, 0);
	return ret;
}

/* pre fw run operations */
int hda_dsp_pre_fw_run(struct snd_sof_dev *sdev)
{
	/* disable clock gating and power gating */
	return hda_dsp_ctrl_clock_power_gating(sdev, false);
}

/* post fw run operations */
int hda_dsp_post_fw_run(struct snd_sof_dev *sdev)
{
	/* re-enable clock gating and power gating */
	return hda_dsp_ctrl_clock_power_gating(sdev, true);
}
