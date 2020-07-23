# sof-intel-vfio

### vfio_api
* File has starter code for setting up VFIO environment as required.
* THis file reads the config space of PCI device. All devices with VFIO-PCI device have 9 regions. The regions can be found in vfio.h. The region indices that work for the audio device are 0, 4, and 7. This is the VFIO_PCI_BAR0_REGION_INDEX, VFIO_PCI_BAR4_REGION_INDEX, and VFIO_PCI_CONFIG_REGION_INDEX respectfully. 
* You can compare the config space this code prints out versus the config space code by running `cat /sys/bus/pci/devices/0000\:00\:1f.3/config | hexdump`. 
* FOr IRQs, there are two ioctls. One to get information, which is already implemented within this file. The other is setting irqs. Here 
* One last important thing to note. In order to make the vfio api to work with the multimedia audio controller, I also had to change the kernel driver of the SMBus interface from the default to the vfio-pci driver. I think here, we can do a sort of passthrough to ensure SMBus interface works as it should.  

### device_to_vfio.sh
* Binds Unbinds devices in group 11 from their default drivers and then binds them to vfio-pci driver