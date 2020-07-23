#include <linux/vfio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

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
	
	
	for (i = 0; i < device_info.num_irqs; i++) {
		struct vfio_irq_info irq = { .argsz = sizeof(irq) };

		irq.index = i;

		ret = ioctl(device, VFIO_DEVICE_GET_IRQ_INFO, &irq);
		if(ret < 0){
			printf("Something went wrong when getting irq info. %d\n", ret);
		}
		else{
			printf("Flags: %u, Count: %u\n",irq.flags, irq.count);
		}

		/* Setup IRQs... eventfds, VFIO_DEVICE_SET_IRQS */
	}


	return 0;
}
