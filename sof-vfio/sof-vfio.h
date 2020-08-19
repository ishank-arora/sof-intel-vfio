#ifndef __SOF_VFIO
#define __SOF_VFIO

#include "common.h"

void vfio_setup(struct dev * info);
void print_region(struct dev * info, unsigned int region);
int map_dma(struct dev * info, unsigned long long size, struct snd_dma_buffer * dma_b);
int unmap_dma(struct dev * info, unsigned long long size, unsigned long long iova);
void * mmap_region(struct dev * info, unsigned int region);
void munmap_region(void * addr, struct dev * info, unsigned int region);
void irq_enable(struct dev * info, unsigned int type);
void irq_disable(struct dev * info, unsigned int type);

#endif