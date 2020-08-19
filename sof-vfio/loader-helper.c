#include <linux/vfio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <sys/mman.h>
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


int cl_stream_prepare(struct dev *info, unsigned int format,
			     unsigned int size, struct snd_dma_buffer *dmab,
			     int direction)
{
	struct hdac_ext_stream *dsp_stream;
	struct hdac_stream *hstream;
	//struct pci_dev *pci = to_pci_dev(sdev->dev);
	int ret;

	if (direction != SNDRV_PCM_STREAM_PLAYBACK) {
		printf("error: code loading DMA is playback only\n");
		return -EINVAL;
	}

	dsp_stream = hda_dsp_stream_get(info, direction);

	if (!dsp_stream) {
		printf("error: no stream available\n");
		return -ENODEV;
	}
	hstream = &dsp_stream->hstream;
	hstream->substream = NULL;

	/* allocate DMA buffer */
	hstream->period_bytes = 0;/* initialize period_bytes */
	hstream->format_val = format;
	hstream->bufsize = size;

	ret = hda_dsp_stream_hw_params(info, dsp_stream, dmab, NULL);
	if (ret < 0) {
		printf("error: hdac prepare failed: %x\n", ret);
		goto error;
	}

	hda_dsp_stream_spib_config(info, dsp_stream, HDA_DSP_SPIB_ENABLE, size);

	return hstream->stream_tag;

error:
	//hda_dsp_stream_put(sdev, direction, hstream->stream_tag);
	unmap_dma(info, size, dmab->addr);
	return ret;
}

/* get next unused stream */
struct hdac_ext_stream *
hda_dsp_stream_get(struct dev *info, int direction)
{
	//struct hdac_bus *bus = sof_to_bus(sdev);
	struct sof_intel_hda_stream *hda_stream;
	struct hdac_ext_stream *stream = NULL;
	struct hdac_stream *s;

	//spin_lock_irq(&bus->reg_lock);

    struct node * n = *info->stream_ref;


	/* get an unused stream */
	while(n != NULL) {
        hda_stream = (struct sof_intel_hda_stream *) n->data;
        stream = &hda_stream->hda_stream;
        s = &stream->hstream;
		if (s->direction == direction && !s->opened) {
			if (hda_stream->host_reserved)
				continue;

			s->opened = true;
			break;
		}
        n = n->next;
	}

	//spin_unlock_irq(&bus->reg_lock);

	/* stream found ? */
	if (!stream)
		printf("error: no free %s streams\n",
			direction == SNDRV_PCM_STREAM_PLAYBACK ?
			"playback" : "capture");

	/*
	 * Disable DMI Link L1 entry when capture stream is opened.
	 * Workaround to address a known issue with host DMA that results
	 * in xruns during pause/release in capture scenarios.
	 */
    if (stream && direction == SNDRV_PCM_STREAM_CAPTURE)
        snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
                    HDA_VS_INTEL_EM2,
                    HDA_VS_INTEL_EM2_L1SEN, 0);

	return stream;
}


	int hda_dsp_stream_hw_params(struct dev *info,
					struct hdac_ext_stream *stream,
					struct snd_dma_buffer *dmab,
					struct snd_pcm_hw_params *params)
{
	//struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_stream *hstream = &stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	int ret, timeout = HDA_DSP_STREAM_RESET_TIMEOUT;
	__u32 dma_start = SOF_HDA_SD_CTL_DMA_START;
	__u32 val, mask;
	__u32 run;

	if (!stream) {
		printf("error: no stream available\n");
		return -ENODEV;
	}

	/* decouple host and link DMA */
	mask = 0x1 << hstream->index;
	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
				mask, mask);

	if (!dmab) {
		printf("error: no dma buffer allocated!\n");
		return -ENODEV;
	}
	/* clear stream status */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_DMA_SD_INT_MASK |
				SOF_HDA_SD_CTL_DMA_START, 0);

	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_HDA_BAR,
					    sd_offset, run,
					    !(run & dma_start),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_STREAM_RUN_TIMEOUT);

	if (ret < 0) {
		printf("error: %s: timeout on STREAM_SD_OFFSET read1\n",
			__func__);
		return ret;
	}

	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_CL_SD_STS,
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_CL_DMA_SD_INT_MASK);

	/* stream reset */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset, 0x1,
				0x1);
	usleep(3);
	do {
		val = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR,
				       sd_offset);
		if (val & 0x1)
			break;
	} while (--timeout);
	if (timeout == 0) {
		printf("error: stream reset failed\n");
		return -ETIMEDOUT;
	}

	timeout = HDA_DSP_STREAM_RESET_TIMEOUT;
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset, 0x1,
				0x0);

	/* wait for hardware to report that stream is out of reset */
	usleep(3);
	do {
		val = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR,
				       sd_offset);
		if ((val & 0x1) == 0)
			break;
	} while (--timeout);
	if (timeout == 0) {
		printf("error: timeout waiting for stream reset\n");
		return -ETIMEDOUT;
	}

	if (hstream->posbuf)
		*hstream->posbuf = 0;

	/* reset BDL address */
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPL,
			  0x0);
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPU,
			  0x0);

	/* clear stream status */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_DMA_SD_INT_MASK |
				SOF_HDA_SD_CTL_DMA_START, 0);

	ret = snd_sof_dsp_read_poll_timeout(info, HDA_DSP_HDA_BAR,
					    sd_offset, run,
					    !(run & dma_start),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_STREAM_RUN_TIMEOUT);

	if (ret < 0) {
		printf("error: %s: timeout on STREAM_SD_OFFSET read2\n",
			__func__);
		return ret;
	}

	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_CL_SD_STS,
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_CL_DMA_SD_INT_MASK);

	hstream->frags = 0;

	ret = hda_dsp_stream_setup_bdl(info, dmab, hstream);
	if (ret < 0) {
		printf("error: set up of BDL failed\n");
		return ret;
	}

	/* program stream tag to set up stream descriptor for DMA */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_SD_CTL_STREAM_TAG_MASK,
				0x7 <<
				SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT);

	/* program cyclic buffer length */
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_CBL,
			  hstream->bufsize);

	/*
	 * Recommended hardware programming sequence for HDAudio DMA format
	 *
	 * 1. Put DMA into coupled mode by clearing PPCTL.PROCEN bit
	 *    for corresponding stream index before the time of writing
	 *    format to SDxFMT register.
	 * 2. Write SDxFMT
	 * 3. Set PPCTL.PROCEN bit for corresponding stream index to
	 *    enable decoupled mode
	 */

	/* couple host and link DMA, disable DSP features */
	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
				mask, 0);

	/* program stream format */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
				sd_offset +
				SOF_HDA_ADSP_REG_CL_SD_FORMAT,
				0xffff, hstream->format_val);

	/* decouple host and link DMA, enable DSP features */
	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
				mask, mask);

	/* program last valid index */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_CL_SD_LVI,
				0xffff, (hstream->frags - 1));

	/* program BDL address */
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPL,
			  (__u32)hstream->bdl.addr);
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_CL_SD_BDLPU,
			  upper_32_bits(hstream->bdl.addr));

	/* enable position buffer */
	if (!(snd_sof_dsp_read(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE)
				& SOF_HDA_ADSP_DPLBASE_ENABLE)) {
		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPUBASE,
				  upper_32_bits(info->posbuf.addr));
		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE,
				  (__u32)info->posbuf.addr |
				  SOF_HDA_ADSP_DPLBASE_ENABLE);
	}

	/* set interrupt enable bits */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_CL_DMA_SD_INT_MASK);

	/* read FIFO size */
	if (hstream->direction == SNDRV_PCM_STREAM_PLAYBACK) {
		hstream->fifo_size =
			snd_sof_dsp_read(info, HDA_DSP_HDA_BAR,
					 sd_offset +
					 SOF_HDA_ADSP_REG_CL_SD_FIFOSIZE);
		hstream->fifo_size &= 0xffff;
		hstream->fifo_size += 1;
	} else {
		hstream->fifo_size = 0;
	}

	return ret;
}

/*
 * set up one of BDL entries for a stream
 */
int hda_setup_bdle(struct dev *info,
			  struct snd_dma_buffer *dmab,
			  struct hdac_stream *stream,
			  struct sof_intel_dsp_bdl **bdlp,
			  int offset, int size, int ioc)
{
	//struct hdac_bus *bus = sof_to_bus(sdev);
	struct sof_intel_dsp_bdl *bdl = *bdlp;

	while (size > 0) {
		dma_addr_t addr;
		int chunk;
		
		if (stream->frags >= HDA_DSP_MAX_BDL_ENTRIES) {
			printf("error: stream frags exceeded\n");
			return -EINVAL;
		}

		addr = dmab->addr + offset;
		/* program BDL addr */
		bdl->addr_l = cpu_to_le32(lower_32_bits(addr));
		bdl->addr_h = cpu_to_le32(upper_32_bits(addr));
		/* program BDL size */
		chunk = snd_sgbuf_get_chunk_size(dmab, offset, size);
		/* one BDLE should not cross 4K boundary */
		if (true) {
			__u32 remain = 0x1000 - (offset & 0xfff);

			if (chunk > remain)
				chunk = remain;
		}
		bdl->size = cpu_to_le32(chunk);
		/* only program IOC when the whole segment is processed */
		size -= chunk;
		bdl->ioc = (size || !ioc) ? 0 : cpu_to_le32(0x01);
		bdl++;
		stream->frags++;
		offset += chunk;

		printf("bdl, frags:%d, chunk size:0x%x;\n",
			 stream->frags, chunk);
	}

	*bdlp = bdl;
	return offset;
}

/*
 * set up Buffer Descriptor List (BDL) for host memory transfer
 * BDL describes the location of the individual buffers and is little endian.
 */
int hda_dsp_stream_setup_bdl(struct dev *info,
			     struct snd_dma_buffer *dmab,
			     struct hdac_stream *stream)
{
	//struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct sof_intel_dsp_bdl *bdl;
	int i, offset, period_bytes, periods;
	int remain, ioc;

	period_bytes = stream->period_bytes;
	printf("period_bytes:0x%x\n", period_bytes);
	if (!period_bytes)
		period_bytes = stream->bufsize;

	periods = stream->bufsize / period_bytes;

	printf("periods:%d bufsz:%d\n", periods, stream->bufsize);

	remain = stream->bufsize % period_bytes;
	if (remain)
		periods++;

	/* program the initial BDL entries */
	bdl = (struct sof_intel_dsp_bdl *)stream->bdl.area;
	offset = 0;
	stream->frags = 0;

	/*
	 * set IOC if don't use position IPC
	 * and period_wakeup needed.
	 */
	//ioc = hda->no_ipc_position ? !stream->no_period_wakeup : 0;

	ioc = !stream->no_period_wakeup;

	for (i = 0; i < periods; i++) {
		if (i == (periods - 1) && remain)
			/* set the last small entry */
			offset = hda_setup_bdle(info, dmab,
						stream, &bdl, offset,
						remain, 0);
		else
			offset = hda_setup_bdle(info, dmab,
						stream, &bdl, offset,
						period_bytes, ioc);
	}

	return offset;
}

int hda_dsp_stream_spib_config(struct dev *info,
			       struct hdac_ext_stream *stream,
			       int enable, __u32 size)
{
	struct hdac_stream *hstream = &stream->hstream;
	__u32 mask;

	if (!info->offsets[HDA_DSP_SPIB_BAR]) {
		printf("error: address of spib capability is NULL\n");
		return -EINVAL;
	}

	mask = (1 << hstream->index);

	/* enable/disable SPIB for the stream */
	snd_sof_dsp_update_bits(info, HDA_DSP_SPIB_BAR,
				SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCCTL, mask,
				enable << hstream->index);

	/* set the SPIB value */
	sof_io_write(info, stream->spib_addr, size);

	return 0;
}


