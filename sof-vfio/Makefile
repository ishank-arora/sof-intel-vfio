CC=x86_64-cros-linux-gnu-clang


all: list.o tools.o sof-vfio.o probe.o driver.o
	$(CC) -o vfio_sof tools.o sof-vfio.o list.o probe.o driver.o
	scp vfio_sof device:~/

tools.o:
	$(CC) -c tools.c

sof-vfio.o:
	$(CC) -c sof-vfio.c

driver.o:
	$(CC) -c driver.c

list.o:
	$(CC) -c list.c 

probe.o:
	$(CC) -c probe.c

clean :
	rm *.o vfio_sof
