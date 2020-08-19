#ifndef __COMMON_H
#define __COMMON_H
#include <stdbool.h>
#include <linux/const.h>
#include <linux/vfio.h>
#include "constants.h"
#include "list.h"
#include "hdaudio.h"

#define DEVICE "0000:00:1f.3"

#define upper_32_bits(n) ((__u32)(((n) >> 16) >> 16))


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
	struct snd_dma_buffer posbuf;
	struct snd_dma_buffer rb;
	struct node ** stream_ref;


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
__u32 snd_sof_dsp_read(struct dev * info, __u32 bar, __u32 offset);
bool snd_sof_dsp_update_bits_unlocked(struct dev * info, __u32 bar, __u32 offset, __u32 mask, __u32 value);
bool snd_sof_dsp_update_bits(struct dev * info, __u32 bar, __u32 offset,
			     __u32 mask, __u32 value);


#define snd_sof_dsp_read_poll_timeout(info, bar, offset, val, cond, sleep_us, timeout_us) \
({ \
	u64 __timeout_us = (timeout_us); \
	unsigned long __sleep_us = (sleep_us); \
	int count = 0; \
	do {							\
		(val) = snd_sof_dsp_read(sdev, bar, offset);		\
		if (cond) { \
			printf("FW Poll Status: reg=%#x successful\n", (val)); \
			break; \
		} \
	} while(count < 100 || (cond))\
	(cond) ? 0 : -ETIMEDOUT; \
})


void get_firmware(struct firmware * fw);

const struct sof_intel_dsp_desc * get_chip_info();

int hda_init(struct dev * info);

int hda_dsp_stream_init(struct dev * info);

int hda_dsp_probe(struct dev * info);

int hda_dsp_ctrl_get_caps(struct dev * info);

void hda_dsp_ctrl_ppcap_enable(struct dev *info, bool enable);

void hda_dsp_ctrl_ppcap_int_enable(struct dev *info, bool enable);



#endif