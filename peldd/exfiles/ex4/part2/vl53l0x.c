// SPDX-License-Identifier: GPL-2.0
/*
 * Simple VL53L0X Time of Flight Sensor.
 *  Based upon: drivers/iio/proximity/vl53l0x-i2c.c
 *
 * ST VL53L0X FlightSense ToF Ranging Sensor on a i2c bus.
 *
 * Copyright (C) 2016 STMicroelectronics Imaging Division.
 * Copyright (C) 2018 Song Qiang <songqiang1304521@gmail.com>
 *
 * Datasheet available at
 * <https://www.st.com/resource/en/datasheet/vl53l0x.pdf>
 *
 * Copyright (C) 2019 Kieran Bingham <kieran@linuxembedded.co.uk>
 *
 * Modified for training purposes by Doulos Ltd, 2019.
 *
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/fs.h>

#include <linux/sysfs.h>

#define VL_REG_SYSRANGE_START				0x00

#define VL_REG_SYSRANGE_MODE_MASK			GENMASK(3, 0)
#define VL_REG_SYSRANGE_MODE_SINGLESHOT			0x00
#define VL_REG_SYSRANGE_MODE_START_STOP			BIT(0)
#define VL_REG_SYSRANGE_MODE_BACKTOBACK			BIT(1)
#define VL_REG_SYSRANGE_MODE_TIMED			BIT(2)
#define VL_REG_SYSRANGE_MODE_HISTOGRAM			BIT(3)

#define VL_REG_RESULT_INT_STATUS			0x13
#define VL_REG_RESULT_RANGE_STATUS			0x14
#define VL_REG_RESULT_RANGE_STATUS_COMPLETE		BIT(0)

/* VL53L0X is an instance of a proximity driver. */
struct vl53l0x {
	struct i2c_client *client;
	unsigned int rate;
};

static int vl53l0x_read_proximity(struct vl53l0x *data,
				  int *val)
{
	struct i2c_client *client = data->client;
	u16 tries = 20;
	u8 buffer[12];
	int ret;

	/*
	 *  Artificial delay to slow down device operations.
	 *  This could instead take the variable, and use it to program a
	 *  feature on the VL53L0x before initiating the range find...
	 */
	unsigned int delay = 1000000 / (data->rate ? data->rate : 1);
	usleep_range(delay, delay + 2000);
	ret = i2c_smbus_write_byte_data(client, VL_REG_SYSRANGE_START, 1);
	if (ret < 0)
		return ret;

	do {
		ret = i2c_smbus_read_byte_data(client,
					       VL_REG_RESULT_RANGE_STATUS);
		if (ret < 0)
			return ret;

		if (ret & VL_REG_RESULT_RANGE_STATUS_COMPLETE)
			break;

		usleep_range(1000, 5000);
	} while (--tries);
	if (!tries)
		return -ETIMEDOUT;

	ret = i2c_smbus_read_i2c_block_data(client, VL_REG_RESULT_RANGE_STATUS,
					    12, buffer);
	if (ret < 0)
		return ret;
	else if (ret != 12) {
		/* Unexpected block size */
		return -EREMOTEIO;
	}

	/* Values should be between 30~1200 in millimeters. */
	*val = (buffer[10] << 8) + buffer[11];

	return 0;
}

static ssize_t proximity_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct vl53l0x *vl53l0x = i2c_get_clientdata(client);
	int ret, val;

	ret = vl53l0x_read_proximity(vl53l0x, &val);
	if (ret < 0) {
		printk("Failed to read proximity sensor\n");
		return ret;
	}

	return sprintf(buf, "Sensor distance : %d\n", val);
}
static DEVICE_ATTR_RO(proximity);

/* probe function declaration */
{
	struct vl53l0x *vl53l0x;
	int ret;
	int val;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK |
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

	vl53l0x = kmalloc(sizeof(*vl53l0x), GFP_KERNEL);
	if (!vl53l0x)
		return -ENOMEM;

	/* Associate the client with our private data */
	i2c_set_clientdata(client, vl53l0x);
	vl53l0x->client = client;

	/* Initialise to a default maximum 10 reads per second */
	vl53l0x->rate = 10;

	ret = vl53l0x_read_proximity(vl53l0x, &val);
	if (ret) {
		printk("Failed to read proximity sensor\n");
		return ret;
	}

	printk("Sensor distance : %d\n", val);

	/* Create a sysfs file on the client device node */
	ret = device_create_file(&client->dev, &dev_attr_proximity);
	if (ret)
		return ret;

	return 0;
}

/* remove function declaration */
{
	struct vl53l0x *vl53l0x = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_proximity);

	kfree(vl53l0x);

	return 0;
}

/* ID table */

/* match table */

/* define i2c driver here */

/* do we need init and exit? */

/* module macros */
