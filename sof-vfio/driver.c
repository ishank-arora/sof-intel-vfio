#include <linux/vfio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include "common.h"
#include "sof-vfio.h"
#include "constants.h"
#include "list.h"



int main(int argc, char ** argv){
	//First we probe.



    //Then we'll turn dsp on


    //Then we'll load firmware

    //Then we'll stream it
    struct dev * info = (struct dev *) calloc(1,sizeof(struct dev));
    vfio_setup(info);
    hda_dsp_probe(info);
	hda_dsp_ctrl_clock_power_gating(info, false);
	hda_dsp_cl_boot_firmware(info);
	hda_dsp_ctrl_clock_power_gating(info, true);

}

