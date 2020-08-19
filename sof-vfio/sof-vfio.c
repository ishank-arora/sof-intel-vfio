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
#include "constants.h"
#include "sof-vfio.h"


ssize_t MIN_DMA_SIZE = PAGE_SIZE; //Can't allocate less than page size

void vfio_setup(struct dev * info){
	int i = 0;

	int ret = -100;

	struct vfio_group_status group_status = {.argsz = sizeof(group_status)};
	
	struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

	CHECK(open("/dev/vfio/vfio", O_RDWR), info->container, 
		"Container did not open properly. Error: %d\n", errno);

	if (ioctl(info->container, VFIO_GET_API_VERSION) != VFIO_API_VERSION){
		/* Unknown API version */
		printf("Unknown API version\n");
	}

	if (!ioctl(info->container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)){
		/* Doesn't support the IOMMU driver we want. */
		printf("Wrong IOMMU version\n");
	}

	/* Open the group */

	CHECK(open("/dev/vfio/11", O_RDWR), info->group, 
		"Group did not open properly. Error: %d\n", errno);
	/* Test the group is viable and available */

	CHECK(ioctl(info->group, VFIO_GROUP_GET_STATUS, &group_status), ret, 
		"getting group status failed. Error: %d\n", errno);

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)){
		/* Group is not viable (ie, not all devices bound for vfio) */
		printf("Not all devices in group are bound vfio\n");
	}
	/* Add the group to the container */
	CHECK(ioctl(info->group, VFIO_GROUP_SET_CONTAINER, &info->container), ret, 
		"adding group to container failed. Error: %d\n", errno);

	/* Enable the IOMMU model we want */
	CHECK(ioctl(info->container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU), ret,
		"Setting IOMMU type failed. Error: %d\n", ret);
	
	/* Get addition IOMMU info */
	CHECK(ioctl(info->container, VFIO_IOMMU_GET_INFO, &iommu_info), ret, 
		"getting iommu info failed. Error: %d\n", errno);


	printf("flags iommu: 0x%08x, page size iommu: 0x%llx\n",iommu_info.flags, iommu_info.iova_pgsizes);

	/* Get a file descriptor for the device */
	CHECK(ioctl(info->group, VFIO_GROUP_GET_DEVICE_FD, DEVICE), info->device, 
		"Device did not open properly. Error: %d\n", errno);

	/* Test and setup the device */
	CHECK(ioctl(info->device, VFIO_DEVICE_GET_INFO, &device_info), ret, 
		"Getting device info failed. error: %d\n", errno);

	printf("Device %s regions flags, regions, irqs: 0x%08x %u %u\n", DEVICE, device_info.flags, device_info.num_regions, device_info.num_irqs);
	for (i = 0; i < device_info.num_regions; i++) {
		struct vfio_region_info reg = { .argsz = sizeof(reg) };

		reg.index = i;

		ret = ioctl(info->device, VFIO_DEVICE_GET_REGION_INFO, &reg);
		if(ret < 0){
			printf("Error getting device region info. %d\n", ret);
			info->flags[i] = 0;
			info->offsets[i] = 0;
			info->sizes[i] = 0;
		}
		else{
			printf("Index:%d , Flags:%u , Size:%llu , Offset:%llx.\n", i, reg.flags, reg.size, reg.offset);
			info->flags[i] = reg.flags;
			info->offsets[i] = reg.offset;
			info->sizes[i] = reg.size;
		}
	}
}

void print_region(struct dev * info, unsigned int region){
	printf("\n\n");
	unsigned long long size = info->sizes[region];
	unsigned long long offset = info->offsets[region];
	unsigned int flag = info->flags[region];
	if(!(flag & VFIO_REGION_INFO_FLAG_READ)){
		printf("Region not readable\n");
		return;
	}
	if(!size){
		printf("Region size is 0\n");
		return;
	}

	int ret = -100;
	void * numbers = malloc(size);
	ret = pread(info->device, numbers, size, offset);
	if(ret < 0){
		printf("read err %d\n", ret);
	}
	else{
		printf("read %d bytes\n", ret);
		sleep(2);
		unsigned short * add  = (unsigned short *) numbers;
		for(int i = 0; i < size/8; i+=2){
			printf("%08x", i*8);
			for(int j = 0; j < 8; j++){
				printf(" %04x", *add);						
				add++;
			}
			printf("\n");
		}
	}
	printf("\n\n");
}

int map_dma(struct dev * info, unsigned long long size, struct snd_dma_buffer * dma_b){
	struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
	/* Allocate some space and setup a DMA mapping */
	
	unsigned long long dma_size = size < MIN_DMA_SIZE ? MIN_DMA_SIZE : size;

	//void * trial = NULL;
	//trial = malloc(size);
	

	//dma_map.vaddr = (unsigned long long) trial;
	dma_map.vaddr = (unsigned long long) mmap(0, dma_size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	dma_map.size = dma_size;
	dma_map.iova = dma_b->addr; /* Size bytes starting at 0xiova from device view */
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	dma_b->bytes = size;
	dma_b->area = (unsigned char *) dma_map.vaddr;

	int ret = -100;

	CHECK(ioctl(info->container, VFIO_IOMMU_MAP_DMA, &dma_map), ret, 
		"IOMMU MAP DMA failed. %d %d\n", errno, ret);

	if(ret < 0){
		return ret;
	}

	return 0;

}

int unmap_dma(struct dev * info, unsigned long long size, unsigned long long iova){
	struct vfio_iommu_type1_dma_unmap dma_unmap = { .argsz = sizeof(dma_unmap) };
	/* Allocate some space and setup a DMA mapping */
	dma_unmap.size = size;
	dma_unmap.iova = iova; /* Size bytes starting at 0xiova from device view */
	dma_unmap.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	int ret = -100;

	CHECK(ioctl(info->container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap), ret, 
		"IOMMU UNMAP DMA failed. %d\n", errno);

	if(ret < 0){
		return ret;
	}

	return 0;
}

void * mmap_region(struct dev * info, unsigned int region){
	unsigned long long region_size = info->sizes[region];
	if(region < VFIO_PCI_NUM_REGIONS && region_size > 0){
		void * ptr;
		CHECK_PTR(mmap(NULL, region_size, PROT_READ | PROT_WRITE, 
			MAP_SHARED, info->device, info->offsets[region]), 
			ptr, "Region mapping failed. Error:%d\n", errno);
		return ptr;
	}
	return NULL;	
}

void munmap_region(void * addr, struct dev * info, unsigned int region){
	unsigned long long region_size = info->sizes[region];
	if(region < VFIO_PCI_NUM_REGIONS && region_size > 0){
		int ret = -100;
		CHECK(msync(addr, region_size, MS_SYNC), ret, 
			"MSYNC failed. Error: %d\n", errno);
		CHECK(munmap(addr, region_size), ret, "munmap failed. Error: %d\n", errno);
	}
	else{
		printf("Unmapping invalid or 0 size region\n");
	}	
}

void irq_enable(struct dev * info, unsigned int type){
	struct vfio_irq_set * irq_setup;
	
	unsigned long size = sizeof(struct vfio_irq_set) + sizeof(int);
	char irq[size];

	irq_setup = (struct vfio_irq_set *) irq;

	irq_setup->argsz = size;
	irq_setup->count = 1;
	irq_setup->index = type;
	irq_setup->flags = VFIO_IRQ_SET_ACTION_TRIGGER | VFIO_IRQ_SET_DATA_EVENTFD;
	irq_setup->start = 0;
	int event_fd = -1;
	CHECK(eventfd(0,0), event_fd, "initialising eventfd failed. error: %d", errno);
	int * pfd;
	pfd = (int *) irq_setup->data;
	*pfd = event_fd;
	int ret  = -100;
	
	CHECK(ioctl(info->device, VFIO_DEVICE_SET_IRQS, irq_setup), ret, "Setting eventfd to irq failed. Error %d", errno);
	info->event_fd = event_fd;
}

void irq_disable(struct dev * info, unsigned int type){
	struct vfio_irq_set * irq_setup;
	
	unsigned long size = sizeof(struct vfio_irq_set) + sizeof(int);
	char irq[size];

	irq_setup = (struct vfio_irq_set *) irq;

	irq_setup->argsz = size;
	irq_setup->count = 1;
	irq_setup->index = type;
	irq_setup->flags = VFIO_IRQ_SET_ACTION_TRIGGER | VFIO_IRQ_SET_DATA_NONE;
	irq_setup->start = 0;
	int ret  = -100;
	
	CHECK(ioctl(info->device, VFIO_DEVICE_SET_IRQS, irq_setup), ret, "Disabling irq failed. Error %d", errno);
}