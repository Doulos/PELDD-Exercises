#!/usr/bin/env python3

import colorsys
import fcntl
import ctypes

# Python IOCTL handling from:
# https://github.com/vpelletier/python-ioctl-opt/blob/master/ioctl_opt/__init__.py
IOC_NONE = 0
IOC_WRITE = 1
IOC_READ = 2

def IOC(dir, type, nr, size):
    """
    dir
        One of IOC_NONE, IOC_WRITE, IOC_READ, or IOC_READ|IOC_WRITE.
        Direction is from the application's point of view, not kernel's.
    size (14-bits unsigned integer)
        Size of the buffer passed to ioctl's "arg" argument.
    """
    _IOC_NRBITS = 8
    _IOC_TYPEBITS = 8
    _IOC_SIZEBITS = 14
    _IOC_DIRBITS = 2

    _IOC_NRSHIFT = 0
    _IOC_TYPESHIFT = _IOC_NRSHIFT + _IOC_NRBITS
    _IOC_SIZESHIFT = _IOC_TYPESHIFT + _IOC_TYPEBITS
    _IOC_DIRSHIFT = _IOC_SIZESHIFT + _IOC_SIZEBITS

    return (dir << _IOC_DIRSHIFT) | (type << _IOC_TYPESHIFT) | (nr << _IOC_NRSHIFT) | (size << _IOC_SIZESHIFT)


class LCD:
    leds = "/sys/class/leds/pca963x:"
    dev = "/dev/lcd"
    width = 16


    def __init__(self):
        def ledPath(colour):
            return self.leds + colour + "/brightness"

        self.lcd = open(self.dev, "w")

        self.red = open(ledPath("red"), "w")
        self.green = open(ledPath("green"), "w")
        self.blue = open(ledPath("blue"), "w")

        self.clear()

    def write(self, msg):
        self.lcd.write(msg)
        self.lcd.flush()

    def clear(self):
        self.lcd.write('\f')
        self.lcd.flush()

    def writeValueBar(self, value, percent):
        width = int(self.width * percent / 100)
        bar = ('=' * width)[:15] + '\n'
        self.clear()
        self.lcd.write("Distance " + value)
        self.lcd.write(bar)
        self.lcd.flush()
        print (value)
        print (bar)

    def setRGB(self, r, g, b):
        def toStr(c):
            return str(int(abs(c)))

        print ("R:" + toStr(r) + " G:" + toStr(g) + " B:" + toStr(b))

        self.red.write(toStr(r))
        self.green.write(toStr(g))
        self.blue.write(toStr(b))
        self.red.flush()
        self.green.flush()
        self.blue.flush()

    # Accepts a float between 0 and 1
    # https://docs.python.org/3/library/colorsys.html
    def setHue(self, hue):
        r, g, b = colorsys.hsv_to_rgb(hue, 1, 1)
        self.setRGB(r * 255, g * 255, b * 255)


class Proximity:
    # define VL_IOC_SET_RATE _IOW('v','s',int32_t*)
    # define VL_IOC_GET_RATE _IOR('v','g',int32_t*)

    VL_IOC_SET_RATE = IOC(IOC_WRITE, ord('v'), ord('s'), 8)
    VL_IOC_GET_RATE = IOC(IOC_READ,  ord('v'), ord('g'), 8)

    def __init__(self):
        self.dev = open("/dev/proximity0", "r")

    def ioctl(self, func, arg, mutate_flag=False):
        ret = ctypes.c_uint()
        ret = fcntl.ioctl(self.dev, func, arg, mutate_flag)
        #if ret < 0:
        #    raise IOError(result)

    def setRate(self, rate):
        value = ctypes.c_uint(rate)
        print("Attempting to set rate of " + str(value))
        print("Using IOCTL " + str(hex(self.VL_IOC_SET_RATE)))
        self.ioctl(self.VL_IOC_SET_RATE, value)

    def getRate(self):
        value = ctypes.c_uint()
        self.ioctl(self.VL_IOC_GET_RATE, value, True)
        return value

    def read(self):
        for line in self.dev:
            yield line


lcd = LCD()

lcd.write("Loading...")
for i in range (1, 255):
    lcd.setHue(i / 255)

vl53l0x = Proximity()
vl53l0x.setRate(2)

print(vl53l0x.getRate())

# Maximum range displayed on the range bar
maxRange = 750

for line in vl53l0x.read():
    value = int(line)
    if value == 8190:
        continue
    percent = value * 100 / maxRange
    lcd.writeValueBar(line, percent)
    lcd.setHue(value / maxRange)

