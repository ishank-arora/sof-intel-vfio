# sof-intel-vfio (on Cannonlake)

### device_to_vfio.sh
* Binds Unbinds devices in group 11 from their default drivers and then binds them to vfio-pci driver


### To load firmware and boot audio DSP
* Look in folder sof-vfio/
* sof-vfio.c: has vfio specific methods, such as setting up vfio on a particular device, memory mampping a region of the device, adding a DMA map to the vfio container so that the attached IOMMU can convert an address from POV of device to the virtual process address. Minimum DMA mapping size must be the page size. 
  * More information in this <a href="https://www.youtube.com/watch?v=WFkdTFTOTpA" target="_blank">talk</a> and in these [slides](helpful/01x04-Alex_Williamson-An_Introduction_to_PCI_Device_Assignment_with_VFIO.pdf) from the same talk
* tools.c: file has tools to read/write and update bits on the device using vfio.
* list.c: basic doubly linked list with functionality to only add to the tail.
* probe.c: methods converted to userspace with the intention of emulating the actions done by the actual SOF device driver during its probe.
* loader.c and loader-helper.c: methods converted to userspace from the actual SOF device driver to emulate loading the firmware and booting the device.
* hdaudio.h: contains stream data type declaration used by the SOF driver, but converted to be relevant in userspace 

#### old/vfio_api.c
* File has starter code for setting up VFIO environment as required.
* THis file reads the config space of PCI device. All devices with VFIO-PCI device have 9 regions. The regions can be found in vfio.h. The region indices that work for the audio device are 0, 4, and 7. This is the VFIO_PCI_BAR0_REGION_INDEX, VFIO_PCI_BAR4_REGION_INDEX, and VFIO_PCI_CONFIG_REGION_INDEX respectfully. 
* You can compare the config space this code prints out versus the config space code by running `cat /sys/bus/pci/devices/0000\:00\:1f.3/config | hexdump`. 
* FOr IRQs, there are two ioctls. One to get information, which is already implemented within this file. The other is setting irqs. Here 
* One last important thing to note. In order to make the vfio api to work with the multimedia audio controller, I also had to change the kernel driver of the SMBus interface from the default to the vfio-pci driver. THis is because, I think, the audio controller device has a SMBus controller function. But to the OS, it appears as it's own device, but as they are both part of the same group, the SMBUs interface had to alse be binded to the vfio-pci driver. I think this will be handled by how we decided to take care of interrupts. 

#### old/vfio_irq.c
WIP, exploring how IRQ handling will work.
* VFIO checks which interrupts are supported by the device.
* VFIO checks whether INTx, MSI, MSIx, Err, and Req interrupts are supported
* For our device, Err IRQ is the only one that is not supported. Or at least ioctl for getting information about the ERR_IRQ fails. This may also be because there have been no errors on the IO-APIC bus. Not sure.
* For INTx, MSI, and Req interrupts, VFIO reports that there is 1 IRQ. For MSIx, there are 0 IRQ. 0 count, according to the documentation, may be used to describe unimplemented interrupt types. According to the documentation, INTx interrupts are enabled by default, and MSI messages are not being transmitted by default. However, the capabilities to disable and enable both exist, so will have to take that into account when setting IRQs. The INTx interrupts have the eventfd, maskable, and automasked flags enabled, while the rest have the eventfd and noresize flag enabled. More information about the flags in the vfio doc. 


### Helpful links
[https://stackoverflow.com/questions/29461518/interrupt-handling-for-assigned-device-through-vfio](https://stackoverflow.com/questions/29461518/interrupt-handling-for-assigned-device-through-vfio)
