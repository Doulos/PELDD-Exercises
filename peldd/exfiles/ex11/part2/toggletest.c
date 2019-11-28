/* Toggletest.c              */
/* Copyright 2019 Doulos Ltd */
/* License GPL V2            */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gpiod.h>

struct toggle_data {
	int value;
	int led;
	int button;
	char *chip;
};

int event_callback(int evtype, unsigned int offset, const struct timespec *unused, void *data)
{
	struct toggle_data *toggle = data;

	int value = toggle->value;

	value = !value;
	printf("led %d callback with value:%d\n", toggle->led, value);
	gpiod_ctxless_set_value(????);

	toggle->value = value;

	return GPIOD_CTXLESS_EVENT_CB_RET_OK;
}


void main(int argc, char *argv[])
{
	struct toggle_data toggle;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <gpio chip name> <led offset> <button offset>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	toggle.value = 0;
	toggle.led = atoi(argv[2]);
	toggle.button = atoi(argv[3]);
	toggle.chip = argv[1];

	printf("GPIO chip: %s\n", toggle.chip);
	printf("LED connected to line: %d\n", toggle.led);
	printf("Button connected to: %d\n", toggle.button);


	gpiod_ctxless_event_monitor(????);

}
