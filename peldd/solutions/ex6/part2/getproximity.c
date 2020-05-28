#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

/* IOCTL definitions should really be in header file */
# define VL_IOC_SET_RATE _IOW('v','s',int32_t*)
# define VL_IOC_GET_RATE _IOR('v','g',int32_t*)


void usage(char *name)
{
	printf("Usage: %s <reads per second>\n", name);
}


int main(int argc, char *argv[])
{
	int ret;
	int value;
	FILE *f;
	int rate;
	int fd;
	int check_rate;

	if (argc != 2) {
		usage(argv[0]);
		return -1;
	}

	rate = atoi(argv[1]);

	printf("Read proximity from file %d times per second\n", rate);

	/* open proximity device file */
	fd = open("/dev/proximity0", O_RDONLY);
	f = fdopen(fd, "r");

	if ((fd == 0)||(f == 0)) {
		perror(argv[0]);
		return -1;
	}

 	/* set value */
        ret = ioctl(fd, VL_IOC_SET_RATE, &rate);
	if (ret < 0)
		perror("set value");
	

        /* check value */
        ret = ioctl(fd, VL_IOC_GET_RATE, &check_rate);
	if (ret < 0)
		perror("get value");

	printf("refresh rate is %d\n", check_rate);

	while (1) {
		ret = fscanf(f, "%d", &value);

		if (ret == 1) {
			printf("distance is %d mm\n", value);
		} else {
			printf("read error\n");
			return -1;
		}
	}

	fclose(f);
	return 0;
}

