#ifndef __HDAUDIO_H
#define __HDAUDIO_H
#include <stdlib.h>
#include <linux/types.h>

typedef unsigned long long dma_addr_t;


struct snd_dma_buffer {
	unsigned char *area;	/* virtual pointer */
	dma_addr_t addr;	/* iova */
	size_t bytes;		/* buffer size in bytes */
};


struct snd_pcm_substream{
	__u64 nothing;
};
/*
 * HD-audio stream
 */
struct hdac_stream {
	struct snd_dma_buffer bdl; /* BDL buffer */
	__le32 *posbuf;		/* position buffer pointer */
	int direction;		/* playback / capture (SNDRV_PCM_STREAM_*) */

	unsigned int bufsize;	/* size of the play buffer in bytes */
	unsigned int period_bytes; /* size of the period in bytes */
	unsigned int frags;	/* number for period in the play buffer */
	unsigned int fifo_size;	/* FIFO size */

	unsigned long long sd_addr;	/* stream descriptor pointer */

	__u32 sd_int_sta_mask;	/* stream int status mask */

	/* pcm support */
	struct snd_pcm_substream *substream;	/* assigned substream,
						 * set in PCM open
						 */
	unsigned int format_val;	/* format value to be set in the
					 * controller and the codec
					 */
	unsigned char stream_tag;	/* assigned stream */
	unsigned char index;		/* stream index */
	int assigned_key;		/* last device# key assigned to */

	bool opened:1;
	bool running:1;
	bool prepared:1;
	bool no_period_wakeup:1;
	bool locked:1;

	/* timestamp */
	unsigned long start_wallclk;	/* start + minimum wallclk */
	unsigned long period_wallclk;	/* wallclk for period */
	//struct timecounter  tc;
	//struct cyclecounter cc;
	int delay_negative_threshold;

	//struct list_head list;
#ifdef CONFIG_SND_HDA_DSP_LOADER
	/* DSP access mutex */
	struct mutex dsp_mutex;
#endif
};




struct hdac_ext_stream {
	struct hdac_stream hstream;

	unsigned long long pphc_addr;
	unsigned long long pplc_addr;

	unsigned long long spib_addr;
	unsigned long long fifo_addr;

	unsigned long long dpibr_addr;

	__u32 dpib;
	__u32 lpib;
	bool decoupled:1;
	bool link_locked:1;
	bool link_prepared;

	struct snd_pcm_substream *link_substream;
};

struct sof_intel_stream {
	size_t posn_offset;
};

struct sof_intel_hda_stream {
	struct dev *info;
	struct hdac_ext_stream hda_stream;
	struct sof_intel_stream stream;
	int host_reserved; /* reserve host DMA channel */
};

#endif