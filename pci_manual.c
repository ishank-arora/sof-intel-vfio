#include <linux/vfio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/pci.h>

int main(){
    struct pci_dev *pci = NULL;
    unsigned short vendor_id = 0x8086;
    unsigned short device_id = 0x02c8;

    while( (pci = pci_get_device(vendor_id, device_id, pci)) != NULL);
    printf("%x\n", pci->vendor);
    


	return 0;
}