#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <poll.h>


void usage(char *name)
{
	printf("Usage: %s <size (1..1024MB) of allocation in MB>\n", name);
}

int count_digits(int value)
{
	int c = 0;
	while (value != 0) {
		c++;
		value /= 10;
	}

	return c;
}


int main(int argc, char *argv[])
{
	int ret;
	unsigned long size = 0;
	int count;
	int fd_input,fd_miscmem;
	char *buf;

	struct pollfd pd[1];

	if (argc != 2) {
		usage(argv[0]);
		goto exit;
	}

	size = atoi(argv[1]);
	
	if ((size <= 0) || (size > 1024)) {
		usage(argv[0]);
		goto exit;
	}

	printf("Test GPIO-Keys and allocating memory in %ld MB chunks\n", size);

	/* generate count value from number of digits */
	count = count_digits(size);

	/* open char device file */
	fd_miscmem = open("/dev/miscmem", O_RDWR);
	if (fd_miscmem < 1) {
		perror("miscmem open");
		goto exit;
	}

	/* send alloc size to kernel */
	ret = write(fd_miscmem, argv[1], count);
	if (ret < 0) {
		perror("write size");
		goto exit;;
	}

	pd[0].events = POLLIN;

	while (1) {

		fd_input = open("/dev/input/event0", O_RDONLY);
		if (fd_input < 1) {
			perror(argv[0]);
			goto exit;
		}

		pd[0].fd = fd_input;

		ret = poll(pd, 1, -1);	
    		
		if (ret < 0) {
        		perror("poll");
        		goto exit;
    		}

		if (pd[0].revents & POLLIN) {
       			printf("input received\n");
			ret = read(fd_miscmem, buf, count);
			if (ret < 0) {
				perror("read");
				goto exit;
			}
		}
		
		usleep(100000);
		close(fd_input);
	}

	exit:
 		close(fd_miscmem);
		close(fd_input);
		printf("%s exiting\n", argv[0]);
		return 0;
}

