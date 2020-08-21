CC=x86_64-cros-linux-gnu-clang

all : vfio_boot.o probe.o
	cc -o vfio_sof vfio_boot.o probe.o
	scp vfio_sof device_to_vfio.sh device:~/

vfio_boot.o : vfio_boot.c common.h
	cc -c vfio_boot.c
probe.o : probe.c
	cc -c probe.c

vfio_api: 
	$(CC) -o vfio_api vfio_api.c
	scp vfio_api device:~/

clean :
	rm *.o vfio_sof vfio_api
