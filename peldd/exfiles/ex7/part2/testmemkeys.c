#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/input.h>

void usage(char *name)
{
	printf("Usage: %s <size (1..1024MB) of allocation in MB>\n", name);
}


int main(int argc, char *argv[])
{
	unsigned long size = 0;
	char* endp;
	int		fd_input = -1 ,
			fd_miscmem = -1;
	size_t	count;
	ssize_t	ret;

	/*------------------------------------*/
	/* parse argument                     */
	/*------------------------------------*/
	if (argc != 2) {
		usage(argv[0]);
		goto exit;
	}

	size = strtol(argv[1], &endp, 10);
	if ((size <= 0) || (size > 1024)) {
		usage(argv[0]);
		goto exit;
	}
	count = endp-argv[1]; /* number of digits of size */

	printf("Test GPIO-Keys and allocating memory in %ld MB chunks\n", size);


	/*------------------------------------*/
	/* setup                              */
	/*------------------------------------*/
	/* open miscmem */
	fd_miscmem = open("/dev/miscmem", O_RDWR);
	if (fd_miscmem == -1) {
		perror("miscmem open");
		goto exit;
	}

	/* send alloc size to kernel */
	ret = write(fd_miscmem, argv[1], count);
	if (ret != (ssize_t) count) {
		perror("write size to miscmem");
		goto exit;
	}

	/* open input event */
	fd_input = open("/dev/input/event0", O_RDONLY);
	if (fd_input == -1) {
		perror("input event0 open");
		goto exit;
	}

	/*------------------------------------*/
	/* processing loop                    */
	/*------------------------------------*/
	while (1) {
		struct input_event ev;

		/* block until an input event is received */
		ret = read(fd_input, &ev, sizeof(struct input_event));
		if (ret<0) {
			perror("read event0");
			goto exit;
		}
		/* printf("input event received %hu %hu %d\n",ev.type,ev.code, ev.value); */

		/* Inform miscmem when switch is operated (KEY_1) */
		if (ev.type == EV_KEY && ev.code == KEY_1 && ev.value==1) {
			char buf;
			ret = read(fd_miscmem, &buf, 1);
			if (ret < 0) {
				perror("read miscmem");
				goto exit;
			}
		}
	}

	/*--------------------------------------------*/
	/* clean-up                                   */
	/*--------------------------------------------*/
	exit:
 		close(fd_miscmem);
		close(fd_input);
		printf("%s exiting\n", argv[0]);
		return 0;
}

