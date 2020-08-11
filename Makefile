boot: vfio_boot.c
	x86_64-cros-linux-gnu-g++ -o vfio_boot vfio_boot.c && scp vfio_boot device:~/

api: vfio_api.c
	x86_64-cros-linux-gnu-g++ -o vfio_api vfio_api.c && scp vfio_api device:~/
