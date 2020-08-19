#ifndef __COMMON_H
#define __COMMON_H
#include <stdbool.h>
#include <linux/const.h>
#include <linux/vfio.h>
#include <linux/byteorder/little_endian.h>
#include "constants.h"
#include "list.h"
#include "hdaudio.h"

#define DEVICE "0000:00:1f.3"

#define upper_32_bits(n) ((__u32)(((n) >> 16) >> 16))

#define lower_32_bits(n) ((__u32)(n))

#define cpu_to_le32 __cpu_to_le32

#define snd_sgbuf_get_chunk_size(dmab, ofs, size)	(size)


#define CHECK(expr, ret, format, ...) do{ \
	ret = expr; \
	if (ret < 0) { \
		printf(format, __VA_ARGS__); \
	} \
} while(0)


#define CHECK_PTR(expr, ptr, format, ...) do{\
	ptr = expr; \
	if((long long )ptr < 0){ \
		printf(format, __VA_ARGS__); \
		ptr = NULL; \
	} \
} while(0)


struct snd_sof_ipc_msg {
	/* message data */
	__u32 header;
	void *msg_data;
	void *reply_data;
	size_t msg_size;
	size_t reply_size;
	int reply_error;

	//wait_queue_head_t waitq;
	bool ipc_complete;
};

/* mailbox descriptor, used for host <-> DSP IPC */
struct snd_sof_mailbox {
	__u32 offset;
	size_t size;
};

/* SOF generic IPC data */
struct snd_sof_ipc {
	struct dev *info;

	/* protects messages and the disable flag */
	//struct mutex tx_mutex;
	/* disables further sending of ipc's */
	bool disable_ipc_tx;

	struct snd_sof_ipc_msg msg;
};



struct dev {
	int container;
	int group;
	int device;
	int event_fd;
	int irq;
	int ipc_irq;
	bool msi_enabled;
	bool use_posbuf;
	unsigned int flags[VFIO_PCI_NUM_REGIONS];
	unsigned long long offsets[VFIO_PCI_NUM_REGIONS];
	unsigned long long sizes[VFIO_PCI_NUM_REGIONS];
	int mmio_bar;
	int mailbox_bar;
	struct snd_dma_buffer dmab;
	struct snd_dma_buffer posbuf;
	struct snd_dma_buffer rb;
	struct node ** stream_ref; /*node stores pointer to sof_intel_hda_stream */


	struct snd_sof_ipc *ipc;
	struct snd_sof_mailbox dsp_box;		/* DSP initiated IPC */
	struct snd_sof_mailbox host_box;	/* Host initiated IPC */
	struct snd_sof_mailbox stream_box;	/* Stream position update */
	struct snd_sof_ipc_msg *msg;
};

struct sof_intel_dsp_desc {
	int cores_num;
	int cores_mask;
	int init_core_mask; /* cores available after fw boot */
	int ipc_req;
	int ipc_req_mask;
	int ipc_ack;
	int ipc_ack_mask;
	int ipc_ctl;
	int rom_init_timeout;
	int ssp_count;			/* ssp count of the platform */
	int ssp_base_offset;		/* base address of the SSPs */
};


struct firmware {
	size_t size;
	const __u8 * data;
};






void snd_sof_dsp_write(struct dev * info, __u32 bar, __u32 offset, __u32 value);
void sof_io_write(struct dev * info, unsigned long long offset, unsigned int size);
__u32 snd_sof_dsp_read(struct dev * info, __u32 bar, __u32 offset);
bool snd_sof_dsp_update_bits_unlocked(struct dev * info, __u32 bar, __u32 offset, __u32 mask, __u32 value);
bool snd_sof_dsp_update_bits(struct dev * info, __u32 bar, __u32 offset,
			     __u32 mask, __u32 value);
void snd_sof_dsp_update_bits_forced(struct dev *info, __u32 bar,
				    __u32 offset, __u32 mask, __u32 value);


#define snd_sof_dsp_read_poll_timeout(info, bar, offset, val, cond, sleep_us, timeout_us) \
({ \
	__u64 __timeout_us = (timeout_us); \
	unsigned long __sleep_us = (sleep_us); \
	int count = 0; \
	do {							\
		(val) = snd_sof_dsp_read(info, bar, offset);		\
		if (cond) { \
			printf("FW Poll Status: reg=%#x successful\n", (val)); \
			break; \
		} \
		count++; \
		usleep(4000); \
	} while(count < 10000 || (cond)); \
	(cond) ? 0 : -ETIMEDOUT; \
})


#define wait_event_timeout(wq_head, condition, timeout)				\
({										\
	long __ret = timeout;							\
	might_sleep();								\
	if (!___wait_cond_timeout(condition))					\
		__ret = __wait_event_timeout(wq_head, condition, timeout);	\
	__ret;									\
})


int load_fw_for_dma(int container, const char *filename, uint64_t iova, struct firmware fw, struct snd_dma_buffer * dmab);

const struct sof_intel_dsp_desc * get_chip_info();

int hda_init(struct dev * info);

int hda_dsp_stream_init(struct dev * info);

int hda_dsp_probe(struct dev * info);

int hda_dsp_ctrl_get_caps(struct dev * info);

void hda_dsp_ctrl_ppcap_enable(struct dev *info, bool enable);

void hda_dsp_ctrl_ppcap_int_enable(struct dev *info, bool enable);

int hda_dsp_cl_boot_firmware(struct dev *info);

int cl_stream_prepare(struct dev *info, unsigned int format,
			     unsigned int size, struct snd_dma_buffer *dmab,
			     int direction);

struct hdac_ext_stream * hda_dsp_stream_get(struct dev *info, int direction);

int hda_dsp_stream_hw_params(struct dev *info,
			     struct hdac_ext_stream *stream,
			     struct snd_dma_buffer *dmab,
			     struct snd_pcm_hw_params *params);

int hda_dsp_stream_setup_bdl(struct dev *info,
			     struct snd_dma_buffer *dmab,
			     struct hdac_stream *stream);

int hda_setup_bdle(struct dev *info,
			  struct snd_dma_buffer *dmab,
			  struct hdac_stream *stream,
			  struct sof_intel_dsp_bdl **bdlp,
			  int offset, int size, int ioc);

int hda_dsp_stream_spib_config(struct dev *info,
			       struct hdac_ext_stream *stream,
			       int enable, __u32 size);


int hda_dsp_core_power_up(struct dev *info, unsigned int core_mask);

int hda_dsp_core_power_down(struct dev *info, unsigned int core_mask);

int hda_dsp_core_run(struct dev *info, unsigned int core_mask);

int hda_dsp_core_reset_leave(struct dev *info, unsigned int core_mask);

int hda_dsp_core_stall_reset(struct dev *info, unsigned int core_mask);

int hda_dsp_core_reset_enter(struct dev *info, unsigned int core_mask);

void hda_dsp_ipc_int_enable(struct dev *info);

void hda_dsp_ipc_int_disable(struct dev *info);

struct hdac_ext_stream *get_stream_with_tag(struct dev *info,
						   int tag);

int cl_dsp_init(struct dev *info, const void *fwdata,
		       __u32 fwsize, int stream_tag);

int cl_copy_fw(struct dev *info, struct hdac_ext_stream *stream);

int cl_trigger(struct dev *info,
		      struct hdac_ext_stream *stream, int cmd);
	
int hda_dsp_stream_trigger(struct dev *info,
			   struct hdac_ext_stream *stream, int cmd);

int hda_dsp_core_reset_power_down(struct dev *info,
				  unsigned int core_mask);

bool hda_dsp_core_is_enabled(struct dev *info,
			     unsigned int core_mask);




#endif