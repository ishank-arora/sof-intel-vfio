#ifndef PROBE_H_
#define PROBE_H_

#include <stdint.h>

int load_fw_for_dma(int container, const char *filename, uint64_t iova);

#endif /* PROBE_H_ */