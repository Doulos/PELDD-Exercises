#!/bin/sh

for f in one two three four five six seven eight nine ten;
	do echo $f | tee /dev/lcd;
done

