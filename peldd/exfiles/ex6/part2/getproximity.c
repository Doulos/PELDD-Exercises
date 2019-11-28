#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void usage(char *name)
{
	printf("Usage: %s\n", name);
}


int main(int argc, char *argv[])
{
	int ret;
	int value;
	FILE *f;


	if (argc != 1) {
		usage(argv[0]);
		return -1;
	}

	printf("Read proximity from file\n");

	/* open proximity device file */
	f = fopen("/dev/proximity0", "r");

	if (f == 0) {
		perror(argv[0]);
		return -1;
	}

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

