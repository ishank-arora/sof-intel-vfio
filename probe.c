#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/vfio.h>

#include <stdio.h>

#include "probe.h"

int vfio_sof_probe()
{
	/* sof_probe_continue */

	/* ---------------------------------
	 * snd_sof_probe -> ops.probe -> hda_dsp_probe
	 * ---------------------------------
	 */

	/* hda_init */
	/* sof_hda_bus_init */
	/* pci_ioremap_bar(pci, 0) */
	/* snd_hdac_ext_bus_init */
	/* hda_dsp_ctrl_get_caps */


	/* pci_ioremap_bar(pci, 4) */
	/* hda_dsp_stream_init */

	/* HDA IRQ: hda_dsp_stream_interrupt, hda_dsp_stream_threaded_handler) */
	/* IPC IRQ: hda_dsp_ipc_irq_handler, ops.irq_thread */
	/* pci_set_master */

	/* hda_init_caps ... */
	/* hda_dsp_ctrl_ppcap_enable */
	/* hda_dsp_ctrl_ppcap_int_enable */

	/* ---------------------------------
	 * snd_sof_run_firmware
	 * ---------------------------------
	 */

	/* snd_sof_dsp_pre_fw_run -> ops.pre_fw_run -> hda_dsp_pre_fw_run
	 * disable clock gating and power gating
	 * hda_dsp_ctrl_clock_power_gating(sdev, false);
	 */

	/* snd_sof_dsp_run -> ops.run -> hda_dsp_cl_boot_firmware */
	/* cl_stream_prepare */
	/* memcpy to dma area */
	/* cl_dsp_init */
	/* cl_copy_fw */
	/* cl_cleanup */

}

/*
 * iova for fw dma
[    7.340624] [2283:cras]hda_dsp_stream_setup_bdl enter
[    7.340628] [2283:cras]    hda_setup_bdle enter
[    7.340630] [2283:cras]        hda_setup_bdle frags = 1, addr = 0xfff40000, size = 131072
[    7.340632] [2283:cras]        hda_setup_bdle frags = 2, addr = 0xfff20000, size = 131072
[    7.340634] [2283:cras]        hda_setup_bdle frags = 3, addr = 0xfff18000, size = 20480
[    7.340636] [2283:cras]    hda_setup_bdle exit
[    7.340637] [2283:cras]hda_dsp_stream_setup_bdl exit
[    7.341809] sof-audio-pci 0000:00:1f.3: error: no reply expected, received 0x0
[    7.436549] sof-audio-pci 0000:00:1f.3: firmware boot complete
 */

int load_fw_for_dma(int container, const char *filename, uint64_t iova)
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

done:
	close(fd);
	return ret;
}
