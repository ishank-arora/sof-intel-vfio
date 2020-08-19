#include <linux/vfio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "sof-vfio.h"
#include "constants.h"
#include "list.h"

#define HDA_FW_BOOT_ATTEMPTS	3




int hda_dsp_cl_boot_firmware(struct dev *info)
{
	//struct snd_sof_pdata *plat_data = sdev->pdata;
	//const struct sof_dev_desc *desc = plat_data->desc;
	const struct sof_intel_dsp_desc *chip_info;
	struct hdac_ext_stream *stream;
	struct firmware stripped_firmware;
	int ret, ret1, tag, i;

	chip_info = get_chip_info();


    unsigned long long address = 0xfff20000;
    struct snd_dma_buffer * dmab = &info->dmab; 
    ret = load_fw_for_dma(info->container, "/lib/firmware/intel/sof/sof-cnl.ri", address, stripped_firmware, dmab);
    if (ret < 0) {
        printf("Firmware loading and setting up dmab failed/n");
    }
	/* init for booting wait */
	//init_waitqueue_head(&sdev->boot_wait);
	//sdev->boot_complete = false;

	/* prepare DMA for code loader stream */
	tag = cl_stream_prepare(info, 0x40, stripped_firmware.size,
				&info->dmab, SNDRV_PCM_STREAM_PLAYBACK);

	if (tag < 0) {
		printf("error: dma prepare for fw loading err: %x\n",
			tag);
		return tag;
	}

	/* get stream with tag */
	stream = get_stream_with_tag(info, tag);
	if (!stream) {
		printf("error: could not get stream with stream tag %d\n",
			tag);
		ret = -ENODEV;
		goto err;
	}

	/* try ROM init a few times before giving up */
	for (i = 0; i < HDA_FW_BOOT_ATTEMPTS; i++) {
        printf("ROM init trying number %d\n", i);
		ret = cl_dsp_init(info, stripped_firmware.data,
				  stripped_firmware.size, tag);

		/* don't retry anymore if successful */
		if (!ret){
            printf("ROM init successful\n");
			break;
        }

		printf("error: Error code=0x%x: FW status=0x%x\n",
			snd_sof_dsp_read(info, HDA_DSP_BAR,
					 HDA_DSP_SRAM_REG_ROM_ERROR),
			snd_sof_dsp_read(info, HDA_DSP_BAR,
					 HDA_DSP_SRAM_REG_ROM_STATUS));
		printf("error: iteration %d of Core En/ROM load failed: %d\n",
			i, ret);
	}

	if (i == HDA_FW_BOOT_ATTEMPTS) {
		printf("error: dsp init failed after %d attempts with err: %d\n",
			i, ret);
		goto cleanup;
	}

	/*
	 * at this point DSP ROM has been initialized and
	 * should be ready for code loading and firmware boot
	 */
	ret = cl_copy_fw(info, stream);
	if (!ret)
		printf("Firmware download successful, booting...\n");
	else
		printf("error: load fw failed ret: %d\n", ret);

cleanup:
// 	/*
// 	 * Perform codeloader stream cleanup.
// 	 * This should be done even if firmware loading fails.
// 	 * If the cleanup also fails, we return the initial error
// 	 */
// 	ret1 = cl_cleanup(sdev, &sdev->dmab, stream);
// 	if (ret1 < 0) {
// 		dev_err(sdev->dev, "error: Code loader DSP cleanup failed\n");

// 		/* set return value to indicate cleanup failure */
// 		if (!ret)
// 			ret = ret1;
// 	}

// 	/*
// 	 * return master core id if both fw copy
// 	 * and stream clean up are successful
// 	 */
// 	if (!ret)
// 		return chip_info->init_core_mask;

// 	/* dump dsp registers and disable DSP upon error */
err:
// 	hda_dsp_dump(sdev, SOF_DBG_REGS | SOF_DBG_PCI | SOF_DBG_MBOX);

// 	/* disable DSP */
// 	snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR,
// 				SOF_HDA_REG_PP_PPCTL,
// 				SOF_HDA_PPCTL_GPROCEN, 0);
// 	return ret;
    return ret;
}


int cl_copy_fw(struct dev *info, struct hdac_ext_stream *stream)
{
	unsigned int reg;
	int ret, status;

	ret = cl_trigger(info, stream, SNDRV_PCM_TRIGGER_START);
	if (ret < 0) {
		printf("error: DMA trigger start failed\n");
		return ret;
	}
    printf("Stream started\n");
    usleep(300000);
	status = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
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
		printf("error: %s: timeout HDA_DSP_SRAM_REG_ROM_STATUS read\n",
			__func__);
	}

	ret = cl_trigger(info, stream, SNDRV_PCM_TRIGGER_STOP);
	if (ret < 0) {
		printf("error: DMA trigger stop failed\n");
		if (!status)
			status = ret;
	}

	return status;
}



struct hdac_ext_stream *get_stream_with_tag(struct dev *info,
						   int tag)
{
	//struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_stream *s;
    struct node * n = *info->stream_ref;

    while(n != NULL){
        struct sof_intel_hda_stream * stream = (struct sof_intel_hda_stream *) n->data;
        s = &stream->hda_stream.hstream;

        if (s->direction == SNDRV_PCM_STREAM_PLAYBACK &&
		    s->stream_tag == tag) {
			return &stream->hda_stream;
		}
        n = n->next;
    }

	return NULL;
}

/*
 * first boot sequence has some extra steps. core 0 waits for power
 * status on core 1, so power up core 1 also momentarily, keep it in
 * reset/stall and then turn it off
 */
int cl_dsp_init(struct dev *info, const void *fwdata,
		       __u32 fwsize, int stream_tag)
{
	//struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = get_chip_info();
	unsigned int status;
	int ret;
	int i;

	/* step 1: power up corex */
	ret = hda_dsp_core_power_up(info, chip->cores_mask);
	if (ret < 0) {
		printf("error: dsp core 0/1 power up failed\n");
		goto err;
	}

	/* DSP is powered up, set all SSPs to slave mode */
	for (i = 0; i < chip->ssp_count; i++) {
		snd_sof_dsp_update_bits_unlocked(info, HDA_DSP_BAR,
						 chip->ssp_base_offset
						 + i * SSP_DEV_MEM_SIZE
						 + SSP_SSC1_OFFSET,
						 SSP_SET_SLAVE,
						 SSP_SET_SLAVE);
	}

	/* step 2: purge FW request */
	snd_sof_dsp_write(info, HDA_DSP_BAR, chip->ipc_req,
			  chip->ipc_req_mask | (HDA_DSP_IPC_PURGE_FW |
			  ((stream_tag - 1) << 9)));

	/* step 3: unset core 0 reset state & unstall/run core 0 */
	ret = hda_dsp_core_run(info, HDA_DSP_CORE_MASK(0));
	if (ret < 0) {
		printf("error: dsp core start failed %d\n", ret);
		ret = -EIO;
		goto err;
	}
    else{
        printf("made it to step 3!\n");
    }

	/* step 4: wait for IPC DONE bit from ROM */
	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
					    chip->ipc_ack, status,
					    ((status & chip->ipc_ack_mask)
						    == chip->ipc_ack_mask),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_INIT_TIMEOUT_US);

	if (ret < 0) {
		printf("error: %s: timeout for HIPCIE done\n",
			__func__);
		goto err;
	}
    else{
        printf("made it to step 4!\n");
    }

	/* set DONE bit to clear the reply IPC message */
	snd_sof_dsp_update_bits_forced(info, HDA_DSP_BAR,
				       chip->ipc_ack,
				       chip->ipc_ack_mask,
				       chip->ipc_ack_mask);

	/* step 5: power down corex */
	ret = hda_dsp_core_power_down(info,
				  chip->cores_mask & ~(HDA_DSP_CORE_MASK(0)));
	if (ret < 0) {
		printf("error: dsp core x power down failed\n");
		goto err;
	}
    else{
        printf("made it to step 5!\n");
    }

	/* step 6: enable IPC interrupts */
	hda_dsp_ipc_int_enable(info);
    printf("made it to step 6!\n");
    

	/* step 7: wait for ROM init */
	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
					HDA_DSP_SRAM_REG_ROM_STATUS, status,
					((status & HDA_DSP_ROM_STS_MASK)
						== HDA_DSP_ROM_INIT),
					HDA_DSP_REG_POLL_INTERVAL_US,
					chip->rom_init_timeout *
					USEC_PER_MSEC);
	if (!ret){
        printf("WHOOOO \n\n");
		return 0;
    }

	printf("error: %s: timeout HDA_DSP_SRAM_REG_ROM_STATUS read\n",
		__func__);

err:
	//hda_dsp_dump(sdev, SOF_DBG_REGS | SOF_DBG_PCI | SOF_DBG_MBOX);
	hda_dsp_core_reset_power_down(info, chip->cores_mask);

	return ret;
}

int hda_dsp_core_reset_power_down(struct dev *info,
				  unsigned int core_mask)
{
	int ret;

	/* place core in reset prior to power down */
	ret = hda_dsp_core_stall_reset(info, core_mask);
	if (ret < 0) {
		printf("error: dsp core reset failed: core_mask %x\n",
			core_mask);
		return ret;
	}

	/* power down core */
	ret = hda_dsp_core_power_down(info, core_mask);
	if (ret < 0) {
		printf("error: dsp core power down fail mask %x: %d\n",
			core_mask, ret);
		return ret;
	}

	/* make sure we are in OFF state */
	if (hda_dsp_core_is_enabled(info, core_mask)) {
		printf("error: dsp core disable fail mask %x: %d\n",
			core_mask, ret);
		ret = -EIO;
	}

	return ret;
}




int hda_dsp_core_power_up(struct dev *info, unsigned int core_mask)
{
	unsigned int cpa;
	__u32 adspcs;
	int ret;

	/* update bits */
	snd_sof_dsp_update_bits(info, HDA_DSP_BAR, HDA_DSP_REG_ADSPCS,
				HDA_DSP_ADSPCS_SPA_MASK(core_mask),
				HDA_DSP_ADSPCS_SPA_MASK(core_mask));

	/* poll with timeout to check if operation successful */
	cpa = HDA_DSP_ADSPCS_CPA_MASK(core_mask);
	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
					    HDA_DSP_REG_ADSPCS, adspcs,
					    (adspcs & cpa) == cpa,
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		printf("error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);
		return ret;
	}

	/* did core power up ? */
	adspcs = snd_sof_dsp_read(info, HDA_DSP_BAR,
				  HDA_DSP_REG_ADSPCS);
	if ((adspcs & HDA_DSP_ADSPCS_CPA_MASK(core_mask)) !=
		HDA_DSP_ADSPCS_CPA_MASK(core_mask)) {
		printf("error: power up core failed core_mask %xadspcs 0x%x\n",
			core_mask, adspcs);
		ret = -EIO;
	}

	return ret;
}

int hda_dsp_core_power_down(struct dev *info, unsigned int core_mask)
{
	__u32 adspcs;
	int ret;

	/* update bits */
	snd_sof_dsp_update_bits_unlocked(info, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_SPA_MASK(core_mask), 0);

	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
				HDA_DSP_REG_ADSPCS, adspcs,
				!(adspcs & HDA_DSP_ADSPCS_SPA_MASK(core_mask)),
				HDA_DSP_REG_POLL_INTERVAL_US,
				HDA_DSP_PD_TIMEOUT * USEC_PER_MSEC);
	if (ret < 0)
		printf("error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);

	return ret;
}

int hda_dsp_core_run(struct dev *info, unsigned int core_mask)
{
	int ret;

	/* leave reset state */
	ret = hda_dsp_core_reset_leave(info, core_mask);
	if (ret < 0)
		return ret;

	/* run core */
	printf("unstall/run core: core_mask = %x\n", core_mask);
	snd_sof_dsp_update_bits_unlocked(info, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_CSTALL_MASK(core_mask),
					 0);

	/* is core now running ? */
	if (!hda_dsp_core_is_enabled(info, core_mask)) {
		hda_dsp_core_stall_reset(info, core_mask);
		printf("error: DSP start core failed: core_mask %x\n",
			core_mask);
		ret = -EIO;
	}

	return ret;
}


int hda_dsp_core_stall_reset(struct dev *info, unsigned int core_mask)
{
	/* stall core */
	snd_sof_dsp_update_bits_unlocked(info, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_CSTALL_MASK(core_mask),
					 HDA_DSP_ADSPCS_CSTALL_MASK(core_mask));

	/* set reset state */
	return hda_dsp_core_reset_enter(info, core_mask);
}

int hda_dsp_core_reset_enter(struct dev *info, unsigned int core_mask)
{
	__u32 adspcs;
	__u32 reset;
	int ret;

	/* set reset bits for cores */
	reset = HDA_DSP_ADSPCS_CRST_MASK(core_mask);
	snd_sof_dsp_update_bits_unlocked(info, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 reset, reset),

	/* poll with timeout to check if operation successful */
	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
					HDA_DSP_REG_ADSPCS, adspcs,
					((adspcs & reset) == reset),
					HDA_DSP_REG_POLL_INTERVAL_US,
					HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		printf("error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);
		return ret;
	}

	/* has core entered reset ? */
	adspcs = snd_sof_dsp_read(info, HDA_DSP_BAR,
				  HDA_DSP_REG_ADSPCS);
	if ((adspcs & HDA_DSP_ADSPCS_CRST_MASK(core_mask)) !=
		HDA_DSP_ADSPCS_CRST_MASK(core_mask)) {
		printf("error: reset enter failed: core_mask %x adspcs 0x%x\n",
			core_mask, adspcs);
		ret = -EIO;
	}

	return ret;
}

int hda_dsp_core_reset_leave(struct dev *info, unsigned int core_mask)
{
	unsigned int crst;
	__u32 adspcs;
	int ret;

	/* clear reset bits for cores */
	snd_sof_dsp_update_bits_unlocked(info, HDA_DSP_BAR,
					 HDA_DSP_REG_ADSPCS,
					 HDA_DSP_ADSPCS_CRST_MASK(core_mask),
					 0);

	/* poll with timeout to check if operation successful */
	crst = HDA_DSP_ADSPCS_CRST_MASK(core_mask);
	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_BAR,
					    HDA_DSP_REG_ADSPCS, adspcs,
					    !(adspcs & crst),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);

	if (ret < 0) {
		printf("error: %s: timeout on HDA_DSP_REG_ADSPCS read\n",
			__func__);
		return ret;
	}

	/* has core left reset ? */
	adspcs = snd_sof_dsp_read(info, HDA_DSP_BAR,
				  HDA_DSP_REG_ADSPCS);
	if ((adspcs & HDA_DSP_ADSPCS_CRST_MASK(core_mask)) != 0) {
		printf("error: reset leave failed: core_mask %x adspcs 0x%x\n",
			core_mask, adspcs);
		ret = -EIO;
	}

	return ret;
}

void hda_dsp_ipc_int_enable(struct dev *info)
{
	//struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = get_chip_info();
    printf("HI!\n");

	/* enable IPC DONE and BUSY interrupts */
	snd_sof_dsp_update_bits(info, HDA_DSP_BAR, chip->ipc_ctl,
			HDA_DSP_REG_HIPCCTL_DONE | HDA_DSP_REG_HIPCCTL_BUSY,
			HDA_DSP_REG_HIPCCTL_DONE | HDA_DSP_REG_HIPCCTL_BUSY);

    printf("interrutps are happening!\n");

	/* enable IPC interrupt */
	snd_sof_dsp_update_bits(info, HDA_DSP_BAR, HDA_DSP_REG_ADSPIC,
				HDA_DSP_ADSPIC_IPC, HDA_DSP_ADSPIC_IPC);

    printf("IPC is happening!\n");
}

void hda_dsp_ipc_int_disable(struct dev *info)
{
	//struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = get_chip_info();

	/* disable IPC interrupt */
	snd_sof_dsp_update_bits(info, HDA_DSP_BAR, HDA_DSP_REG_ADSPIC,
				HDA_DSP_ADSPIC_IPC, 0);

	/* disable IPC BUSY and DONE interrupt */
	snd_sof_dsp_update_bits(info, HDA_DSP_BAR, chip->ipc_ctl,
			HDA_DSP_REG_HIPCCTL_BUSY | HDA_DSP_REG_HIPCCTL_DONE, 0);
}


int cl_trigger(struct dev *info,
		      struct hdac_ext_stream *stream, int cmd)
{
	struct hdac_stream *hstream = &stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);

	/* code loader is special case that reuses stream ops */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		usleep(300000);

		snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
					1 << hstream->index,
					1 << hstream->index);

		snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
					sd_offset,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK);

		hstream->running = true;
		return 0;
	default:
		return hda_dsp_stream_trigger(info, stream, cmd);
	}
}

int hda_dsp_stream_trigger(struct dev *info,
			   struct hdac_ext_stream *stream, int cmd)
{
	struct hdac_stream *hstream = &stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	__u32 dma_start = SOF_HDA_SD_CTL_DMA_START;
	int ret;
	__u32 run;

	/* cmd must be for audio stream */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_START:
		snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
					1 << hstream->index,
					1 << hstream->index);

		snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
					sd_offset,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK);

		ret = snd_sof_dsp_read_poll_timeout(info,
					HDA_DSP_HDA_BAR,
					sd_offset, run,
					((run &	dma_start) == dma_start),
					HDA_DSP_REG_POLL_INTERVAL_US,
					HDA_DSP_STREAM_RUN_TIMEOUT);

		if (ret < 0) {
			printf("error: %s: cmd %d: timeout on STREAM_SD_OFFSET read\n",
				__func__, cmd);
			return ret;
		}

		hstream->running = true;
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
					sd_offset,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK, 0x0);

		ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_HDA_BAR,
						sd_offset, run,
						!(run &	dma_start),
						HDA_DSP_REG_POLL_INTERVAL_US,
						HDA_DSP_STREAM_RUN_TIMEOUT);

		if (ret < 0) {
			printf("error: %s: cmd %d: timeout on STREAM_SD_OFFSET read\n",
				__func__, cmd);
			return ret;
		}

		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, sd_offset +
				  SOF_HDA_ADSP_REG_CL_SD_STS,
				  SOF_HDA_CL_DMA_SD_INT_MASK);

		hstream->running = false;
		snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
					1 << hstream->index, 0x0);
		break;
	default:
		printf("error: unknown command: %d\n", cmd);
		return -EINVAL;
	}

	return 0;
}

bool hda_dsp_core_is_enabled(struct dev *info,
			     unsigned int core_mask)
{
	int val;
	bool is_enable;

	val = snd_sof_dsp_read(info, HDA_DSP_BAR, HDA_DSP_REG_ADSPCS);

	is_enable = ((val & HDA_DSP_ADSPCS_CPA_MASK(core_mask)) &&
			(val & HDA_DSP_ADSPCS_SPA_MASK(core_mask)) &&
			!(val & HDA_DSP_ADSPCS_CRST_MASK(core_mask)) &&
			!(val & HDA_DSP_ADSPCS_CSTALL_MASK(core_mask)));

	printf("DSP core(s) enabled? %d : core_mask %x\n",
		is_enable, core_mask);

	return is_enable;
}


// void hda_dsp_dump(struct dev *info, __u32 flags)
// {
// 	struct sof_ipc_dsp_oops_xtensa xoops;
// 	struct sof_ipc_panic_info panic_info;
// 	u32 stack[HDA_DSP_STACK_DUMP_SIZE];
// 	u32 status, panic;

// 	/* try APL specific status message types first */
// 	hda_dsp_get_status(sdev);

// 	/* now try generic SOF status messages */
// 	status = snd_sof_dsp_read(info, HDA_DSP_BAR,
// 				  HDA_DSP_SRAM_REG_FW_STATUS);
// 	panic = snd_sof_dsp_read(info, HDA_DSP_BAR, HDA_DSP_SRAM_REG_FW_TRACEP);

// 	if (sdev->boot_complete) {
// 		hda_dsp_get_registers(sdev, &xoops, &panic_info, stack,
// 				      HDA_DSP_STACK_DUMP_SIZE);
// 		snd_sof_get_status(sdev, status, panic, &xoops, &panic_info,
// 				   stack, HDA_DSP_STACK_DUMP_SIZE);
// 	} else {
// 		dev_err(sdev->dev, "error: status = 0x%8.8x panic = 0x%8.8x\n",
// 			status, panic);
// 		hda_dsp_get_status(sdev);
// 	}
// }




int load_fw_for_dma(int container, const char *filename, uint64_t iova, struct firmware fw, struct snd_dma_buffer * dmab)
{
	struct stat stat_buf;
	struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
	int fd, ret;
	ssize_t read_size, n;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		printf("Failed to open %s: %s\n", filename, strerror(errno));
		return fd;
	}

	ret = fstat(fd, &stat_buf);
	if (ret < 0) {
		printf("Failed to stat %s: %s\n", filename, strerror(errno));
		goto done;
	}

	dma_map.size = stat_buf.st_size;
	dma_map.vaddr = (__u64)mmap(0, dma_map.size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	dma_map.iova = iova;
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
	if (ret < 0) {
		printf("Failed VFIO_IOMMU_MAP_DMA: %s\n", strerror(errno));
		goto done;
	}

	read_size = 0;
	while (read_size < stat_buf.st_size) {
		n = read(fd, (void *)dma_map.vaddr,
				stat_buf.st_size - read_size);
		if (n == 0)
			break;
		if (n < 0) {
			printf("Failed to read %s: %s\n", filename,
					strerror(errno));
			goto done;
		}
		read_size += n;
	}
	printf("Loaded %s (%zd bytes) to IOVA 0x%lx\n",
			filename, read_size, iova);

    fw.data = (const unsigned char *) dma_map.vaddr;
    fw.size = dma_map.size;
    dmab->addr = iova;
    dmab->area = (unsigned char *) dma_map.vaddr;
    dmab->bytes = dma_map.size;

done:
	close(fd);
	return ret;
}















