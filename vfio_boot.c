#include <linux/vfio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "common.h"








struct snd_sof_fw_header {
	unsigned char sig[SND_SOF_FW_SIG_SIZE]; /* "Reef" */
	__u32 file_size;	/* size of file minus this header */
	__u32 num_modules;	/* number of modules */
	__u32 abi;		/* version of header format */
} __packed;



int main() {
	dev * info = (dev *) malloc(sizeof(dev));
	info->container = -1;
	info->group = -1;
	info->device = -1;
	int i = 0;
	struct vfio_group_status group_status = {.argsz = sizeof(group_status)};
	
	struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
	struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
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
	int ret = -100;
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

	/* Allocate some space and setup a DMA mapping */
	dma_map.vaddr = (__u64) mmap(0, 1024*1024, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	dma_map.size = 1024*1024;
	dma_map.iova = 0; /* 1MB starting at 0x0 from device view */
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	ret = ioctl(info->container, VFIO_IOMMU_MAP_DMA, &dma_map);
	if(ret < 0){
		printf("IOMMU MAP DMA failed. %d\n", ret);
	}

	printf("DMA Map info. Size: %llu, flags: %u\n", dma_map.size, dma_map.flags);

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

    const char * fw_file = "/lib/firmware/intel/sof/sof-cnl.ri";
    struct snd_sof_fw_header * header;
    size_t fw_size = sizeof(*header);
    __u8 * data = (__u8 *) malloc(fw_size);
    int f = open(fw_file, O_RDWR);
    FILE * fp;
    fp = fopen(fw_file, "r");
    fseek(fp, 0L, SEEK_END);
    long actual_size = ftell(fp);
    rewind(fp);
    fclose(fp);
    printf("\n\nActual size: %ld\n\n", actual_size);
    if(f < 0){
        printf("opening firmware file failed\n");
    }
    else{
        ret = read(f, data, actual_size);
        if(ret < 0){
            printf("Read firmware failed\n");
        }
    }
	firmware * fw = (firmware *) malloc(sizeof(firmware));
	fw->data = data;
	fw->size = actual_size;


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



	return 0;
}
























__u32 snd_sof_dsp_read(dev * info, __u32 bar, __u32 offset){
	int device = info->device;
	int ret;

	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	reg.index = bar;

	ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if(ret < 0){
		printf("Error getting device region. %d\n", ret);
	}
	else{
		__u32 * result = (__u32 *) malloc(sizeof(__u32));
		ret = pread(device, result, sizeof(__u32), reg.offset+offset);
		if(ret < 0){
			printf("read err in dsp_read %d\n", ret);
		}
		else{
			return *result;		
		}
	}
	printf("read Didnt work\n");
	return 0;
	
}

void snd_sof_dsp_write(dev * info, __u32 bar, __u32 offset, __u32 value){
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


bool snd_sof_dsp_update_bits(dev * info, __u32 bar, __u32 offset,
			     __u32 mask, __u32 value)
{

	bool change = snd_sof_dsp_update_bits_unlocked(info, bar, offset, mask,
						  value);
	return change;
}


bool snd_sof_dsp_update_bits_unlocked(dev * info, __u32 bar, __u32 offset, __u32 mask, __u32 value)
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