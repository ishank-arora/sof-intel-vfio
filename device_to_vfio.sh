#!/bin/bash

echo 0000:00:1f.3 > /sys/bus/pci/devices/0000\:00\:1f.3/driver/unbind
echo 8086 02c8 > /sys/bus/pci/drivers/vfio-pci/new_id
echo 0000:00:1f.4 > /sys/bus/pci/devices/0000\:00\:1f.4/driver/unbind
echo 8086 02a3 > /sys/bus/pci/drivers/vfio-pci/new_id
chown $USER:$USER /dev/vfio/11
echo $USER
echo 1 > /sys/module/vfio_iommu_type1/parameters/allow_unsafe_interrupts
