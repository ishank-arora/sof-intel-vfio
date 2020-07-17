#include <linux/vfio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#define DMA_SIZE 19408

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
	int checker = -100;
	/* Test the group is viable and available */
	checker = ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);
	if(checker < 0){
		printf("getting group status failed. Error: %d\n", checker);
	}
	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)){
		/* Group is not viable (ie, not all devices bound for vfio) */
		printf("group not ready for this\n");
	}
	/* Add the group to the container */
	checker = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
	if(checker < 0){
		printf("adding group to container failed. Error: %d\n", checker);
	}
	/* Enable the IOMMU model we want */
	checker = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if(checker < 0){
		printf("Setting IOMMU type failed. Error: %d\n", checker);
	}
	iommu_info.iova_pgsizes = 1324u;
	/* Get addition IOMMU info */
	checker = ioctl(container, VFIO_IOMMU_GET_INFO, &iommu_info);
	if(checker < 0){
		printf("getting iommu info failed. Error: %d\n", checker);
	}
	printf("flags iommu: %u\n",iommu_info.flags);

	/* Allocate some space and setup a DMA mapping */
	dma_map.vaddr = (__u64) mmap(0, 1024*1024, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	dma_map.size = 1024*1024;
	dma_map.iova = 0; /* 1MB starting at 0x0 from device view */
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	checker = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);
	if(checker < 0){
		printf("I wonder if this means something. %d\n", checker);
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
	checker = ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);
	if(checker < 0){
		printf("Getting device info failed. error: %d\n", checker);
	}
	printf("Device 3 regions flags and regions: %u %u\n", device_info.flags, device_info.num_regions);

	for (i = 0; i < device_info.num_regions; i++) {
		struct vfio_region_info reg = { .argsz = sizeof(reg) };

		reg.index = i;

		checker = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);
		if(checker < 0){
			printf("Something went wrong when getting device region. %d\n", checker);
		}
		else{
			printf("Index:%d , Flags:%u , Size:%llu , Offset:%llu.\n", i, reg.flags, reg.size, reg.offset);
		}

		/* Setup mappings... read/write offsets, mmaps
		 * For PCI devices, config space is a region */
	}
	printf("%u\n", *((unsigned int*)dma_map.vaddr));

	// for (i = 0; i < device_info.num_irqs; i++) {
	// 	struct vfio_irq_info irq = { .argsz = sizeof(irq) };

	// 	irq.index = i;

	// 	ioctl(device, VFIO_DEVICE_GET_IRQ_INFO, &irq);

	// 	/* Setup IRQs... eventfds, VFIO_DEVICE_SET_IRQS */
	// }


	return 0;
}
