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
#include "common.h"
#include "sof-vfio.h"
#include "constants.h"
#include "list.h"

int hda_dsp_probe(struct dev * info)
{
    int ret = -100;
    info->mmio_bar = HDA_DSP_BAR;
    info->mailbox_bar = HDA_DSP_BAR;
    ret = hda_init(info);
    if(ret < 0)
        printf("hda_init failed\n");

    ret = hda_dsp_stream_init(info);
    if(ret < 0)
        printf("hda_dsp_stream_init failed\n");


	bool hda_use_msi = true;

	if (hda_use_msi) {
		printf("use msi interrupt mode\n");
		irq_enable(info, VFIO_PCI_MSI_IRQ_INDEX);
		info->irq = info->event_fd;
		/* ipc irq number is the same of hda irq */
		info->ipc_irq = info->irq;
		/* initialised to "false" by kzalloc() */
		info->msi_enabled = true;
	}
	else{
		printf("use legacy interrupt mode\n");
		irq_enable(info, VFIO_PCI_INTX_IRQ_INDEX);
		info->irq = info->event_fd;
		/* ipc irq number is the same of hda irq */
		info->ipc_irq = info->irq;
		/* initialised to "false" by kzalloc() */
		info->msi_enabled = true;
	}


	/*
	 * clear TCSEL to clear playback on some HD Audio
	 * codecs. PCI TCSEL is defined in the Intel manuals.
	 */
	snd_sof_dsp_update_bits(info, VFIO_PCI_CONFIG_REGION_INDEX, PCI_TCSEL, 0x07, 0);

	struct hdac_stream *stream;
	struct node * n = *info->stream_ref;
	struct sof_intel_hda_stream * hda_stream;
	int sd_offset = 0;
	
	while(n != NULL) {
		hda_stream = (struct sof_intel_hda_stream *) n->data;
		stream = &hda_stream->hda_stream.hstream;
		sd_offset = SOF_STREAM_SD_OFFSET(stream);
		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR,
				  sd_offset + SOF_HDA_ADSP_REG_CL_SD_STS,
				  SOF_HDA_CL_DMA_SD_INT_MASK);
		n = n->next;
	}

	/* clear WAKESTS */
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_WAKESTS,
			  SOF_HDA_WAKESTS_INT_MASK);

	/* clear interrupt status register */
	snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_INTSTS,
			  SOF_HDA_INT_CTRL_EN | SOF_HDA_INT_ALL_STREAM);

	/* enable CIE and GIE interrupts */
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
				SOF_HDA_INT_CTRL_EN | SOF_HDA_INT_GLOBAL_EN,
				SOF_HDA_INT_CTRL_EN | SOF_HDA_INT_GLOBAL_EN);

	info->use_posbuf = true;
	/* program the position buffer */
	if (info->use_posbuf && info->posbuf.addr) {
		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE,
				  (__u32)info->posbuf.addr);
		snd_sof_dsp_write(info, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPUBASE,
				  upper_32_bits(info->posbuf.addr));
	}
	
	hda_dsp_ctrl_ppcap_enable(info, true);
	hda_dsp_ctrl_ppcap_int_enable(info, true);

	info->dsp_box.offset = HDA_DSP_MBOX_UPLINK_OFFSET;

    return ret;
}

int hda_dsp_ctrl_clock_power_gating(struct dev *info, bool enable)
{
	__u32 val;

	/* enable/disable audio dsp clock gating */
	val = enable ? PCI_CGCTL_ADSPDCGE : 0;
	snd_sof_pci_update_bits(info, PCI_CGCTL, PCI_CGCTL_ADSPDCGE, val);

	/* enable/disable DMI Link L1 support */
	val = enable ? HDA_VS_INTEL_EM2_L1SEN : 0;
	snd_sof_dsp_update_bits(info, HDA_DSP_HDA_BAR, HDA_VS_INTEL_EM2,
				HDA_VS_INTEL_EM2_L1SEN, val);

	/* enable/disable audio dsp power gating */
	val = enable ? 0 : PCI_PGCTL_ADSPPGD;
	snd_sof_pci_update_bits(info, PCI_PGCTL, PCI_PGCTL_ADSPPGD, val);

	return 0;
}

int hda_init(struct dev * info)
{
	/* get controller capabilities */
    int ret = -100;
	ret = hda_dsp_ctrl_get_caps(info);
	if (ret < 0)
		printf("error: get caps error\n");

    info->stream_ref = (struct node **) malloc(sizeof(struct node *));
    *info->stream_ref = NULL;

	return ret;
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
            info->offsets[HDA_DSP_PP_BAR] = info->offsets[HDA_DSP_HDA_BAR] + offset;
			break;
		case SOF_HDA_SPIB_CAP_ID:
			printf("found SPIB capability at 0x%x\n",
				offset);
            info->offsets[HDA_DSP_SPIB_BAR] = info->offsets[HDA_DSP_HDA_BAR] + offset;
			break;
		case SOF_HDA_DRSM_CAP_ID:
			printf("found DRSM capability at 0x%x\n",
				offset);
            info->offsets[HDA_DSP_DRSM_BAR] = info->offsets[HDA_DSP_HDA_BAR] + offset;
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


int hda_dsp_stream_init(struct dev * info)
{
    //struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_ext_stream *stream;
	struct hdac_stream *hstream;
    //struct pci_dev *pci = to_pci_dev(sdev->dev);
	//struct sof_intel_hda_dev *sof_hda = bus_to_sof_hda(bus);
	int sd_offset;
	int i, num_playback, num_capture, num_total, ret;
	__u32 gcap;

	gcap = snd_sof_dsp_read(info, HDA_DSP_HDA_BAR, SOF_HDA_GCAP);
	printf("hda global caps = 0x%x\n", gcap);

	/* get stream count from GCAP */
	num_capture = (gcap >> 8) & 0x0f;
	num_playback = (gcap >> 12) & 0x0f;
	num_total = num_playback + num_capture;

	printf("detected %d playback and %d capture streams\n",
		num_playback, num_capture);

	if (num_playback >= SOF_HDA_PLAYBACK_STREAMS) {
		printf("error: too many playback streams %d\n",
			num_playback);
		return -EINVAL;
	}

	if (num_capture >= SOF_HDA_CAPTURE_STREAMS) {
		printf("error: too many capture streams %d\n",
			num_playback);
		return -EINVAL;
	}

	/*
	 * mem alloc for the position buffer
	 * TODO: check position buffer update
	 */
	info->posbuf.addr = 0xf000;
	ret = map_dma(info, SOF_HDA_DPIB_ENTRY_SIZE * num_total, &info->posbuf);
	if (ret < 0) {
		printf("error: posbuffer dma alloc failed\n");
		return -ENOMEM;
	}

	info->rb.addr = 0x10000;
    ret = map_dma(info, PAGE_SIZE, &info->rb);
	if (ret < 0) {
		printf("error: ring buffer dma alloc failed\n");
		return -ENOMEM;
	}

	/* create capture streams */
	for (i = 0; i < num_capture; i++) {
		
		struct sof_intel_hda_stream *hda_stream;

		hda_stream = calloc(1, sizeof(*hda_stream));
		if (!hda_stream)
			return -ENOMEM;

		hda_stream->info = info;

		stream = &hda_stream->hda_stream;

		stream->pphc_addr = info->offsets[HDA_DSP_PP_BAR] +
			SOF_HDA_PPHC_BASE + SOF_HDA_PPHC_INTERVAL * i;

		stream->pplc_addr = info->offsets[HDA_DSP_PP_BAR] +
			SOF_HDA_PPLC_BASE + SOF_HDA_PPLC_MULTI * num_total +
			SOF_HDA_PPLC_INTERVAL * i;

		/* do we support SPIB */
		if (info->offsets[HDA_DSP_SPIB_BAR]) {
			stream->spib_addr = info->offsets[HDA_DSP_SPIB_BAR] +
				SOF_HDA_SPIB_BASE + SOF_HDA_SPIB_INTERVAL * i +
				SOF_HDA_SPIB_SPIB;

			stream->fifo_addr = info->offsets[HDA_DSP_SPIB_BAR] +
				SOF_HDA_SPIB_BASE + SOF_HDA_SPIB_INTERVAL * i +
				SOF_HDA_SPIB_MAXFIFO;
		}

		hstream = &stream->hstream;
		hstream->sd_int_sta_mask = 1 << i;
		hstream->index = i;
		sd_offset = SOF_STREAM_SD_OFFSET(hstream);
		hstream->sd_addr = info->offsets[HDA_DSP_HDA_BAR] + sd_offset;
		hstream->stream_tag = i + 1;
		hstream->opened = false;
		hstream->running = false;
		hstream->direction = SNDRV_PCM_STREAM_CAPTURE;

		/* memory alloc for stream BDL */
		hstream->bdl.addr = 0x11000 + i*PAGE_SIZE;
		ret = map_dma(info, HDA_DSP_BDL_SIZE, &hstream->bdl);
		if (ret < 0) {
			printf("error: stream bdl dma alloc failed\n");
			return -ENOMEM;
		}
		hstream->posbuf = (__le32 *)(info->posbuf.area +
			(hstream->index) * 8);
		list_add_tail(info->stream_ref, hda_stream);
	}

	/* create playback streams */
	for (i = num_capture; i < num_total; i++) {
		struct sof_intel_hda_stream *hda_stream;

		hda_stream = calloc(1, sizeof(*hda_stream));
		if (!hda_stream)
			return -ENOMEM;

		hda_stream->info = info;

		stream = &hda_stream->hda_stream;

		stream->pphc_addr = info->offsets[HDA_DSP_PP_BAR] +
			SOF_HDA_PPHC_BASE + SOF_HDA_PPHC_INTERVAL * i;

		stream->pplc_addr = info->offsets[HDA_DSP_PP_BAR] +
			SOF_HDA_PPLC_BASE + SOF_HDA_PPLC_MULTI * num_total +
			SOF_HDA_PPLC_INTERVAL * i;

		/* do we support SPIB */
		if (info->offsets[HDA_DSP_SPIB_BAR]) {
			stream->spib_addr = info->offsets[HDA_DSP_SPIB_BAR] +
				SOF_HDA_SPIB_BASE + SOF_HDA_SPIB_INTERVAL * i +
				SOF_HDA_SPIB_SPIB;

			stream->fifo_addr = info->offsets[HDA_DSP_SPIB_BAR] +
				SOF_HDA_SPIB_BASE + SOF_HDA_SPIB_INTERVAL * i +
				SOF_HDA_SPIB_MAXFIFO;
		}

		hstream = &stream->hstream;
		hstream->sd_int_sta_mask = 1 << i;
		hstream->index = i;
		sd_offset = SOF_STREAM_SD_OFFSET(hstream);
		hstream->sd_addr = info->offsets[HDA_DSP_HDA_BAR] + sd_offset;
		hstream->stream_tag = i + 1;
		hstream->opened = false;
		hstream->running = false;
		hstream->direction = SNDRV_PCM_STREAM_PLAYBACK;

		/* memory alloc for stream BDL */
		hstream->bdl.addr = 0x11000 + i*PAGE_SIZE;
		ret = map_dma(info, HDA_DSP_BDL_SIZE, &hstream->bdl);
		if (ret < 0) {
			printf("error: stream bdl dma alloc failed\n");
			return -ENOMEM;
		}
		hstream->posbuf = (__le32 *)(info->posbuf.area +
			(hstream->index) * 8);

		list_add_tail(info->stream_ref, hda_stream);
	}

	/* store total stream count (playback + capture) from GCAP */
	//sof_hda->stream_max = num_total;

	return 0;
}

void hda_dsp_ctrl_ppcap_enable(struct dev *info, bool enable)
{
	__u32 val = enable ? SOF_HDA_PPCTL_GPROCEN : 0;

	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
				SOF_HDA_PPCTL_GPROCEN, val);
}

void hda_dsp_ctrl_ppcap_int_enable(struct dev *info, bool enable)
{
	__u32 val	= enable ? SOF_HDA_PPCTL_PIE : 0;

	snd_sof_dsp_update_bits(info, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
				SOF_HDA_PPCTL_PIE, val);
}
