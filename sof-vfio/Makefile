CC=x86_64-cros-linux-gnu-clang
DEPS=list.o tools.o sof-vfio.o probe.o loader.o loader-helper.o driver.o


all: $(DEPS)
	$(CC) -o vfio_sof $(DEPS)
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

loader.o:
	$(CC) -c loader.c

loader-helper.o:
	$(CC) -c loader-helper.c

clean :
	rm *.o vfio_sof
