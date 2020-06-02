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
 * Modified for training purposes by Doulos Ltd 2019
*/

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/fs.h>

#include <linux/ioctl.h>

#include <linux/debugfs.h>

#define VL_IOC_SET_RATE _IOW('v','s',int32_t*)
#define VL_IOC_GET_RATE _IOR('v','g',int32_t*)

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

/*
 * A global framework for proximity devices.
 */
static struct proximity_framework {
	/* A class of drivers for devices and sysfs framework */
	struct class *class;

	/* All proximity devices will have this major device number */
	unsigned int major;

	void *drvdata[255];
} proximity;

/* Simple device registration */
#define PROXIMITY_DATA(minor) proximity.drvdata[(minor)]
#define REGISTER_PROXIMITY(minor, private) PROXIMITY_DATA((minor)) = (private)

/* VL53L0X is an instance of a proximity driver. */
struct vl53l0x {
	struct device *dev;
	struct i2c_client *client;

	unsigned int minor;

	unsigned int rate;
};

/* Debugfs data */
static struct dentry *proximity_data, *proximity_rate;

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

static int vl53l0x_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);

	/* Identify this device, and associate the device data with the file */
	file->private_data = PROXIMITY_DATA(minor);

	/* nonseekable_open removes seek, and pread()/pwrite() permissions */
	return nonseekable_open(inode, file);
}

static ssize_t vl53l0x_read(struct file *file, char __user *buf, size_t count, loff_t * ppos)
{
	struct vl53l0x *data = file->private_data;
	int ret, val;
	char str[32];

	ret = vl53l0x_read_proximity(data, &val);
	if (ret)
		return ret;

	ret = snprintf(str, sizeof(str), "%d\n", val);

	if (ret>count)
		/* inform user that buf is too small */
		ret = -EINVAL;
	else if (copy_to_user(buf, str, ret))
		ret = -EFAULT;
	
	return ret;
}

static long vl53l0x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct vl53l0x *data = file->private_data;
	int value = 0;

	switch(cmd) {
	case VL_IOC_SET_RATE:
		if (copy_from_user(&value ,(int32_t*) arg, sizeof(value)))
			return -EFAULT;

		dev_info(data->dev, "Setting rate at %d\n", value);
		data->rate = value;

		break;
	case VL_IOC_GET_RATE:
		value = data->rate;

		if (copy_to_user((int32_t*) arg, &value, sizeof(value)))
			return -EFAULT;

		break;
	default:
		 /* ENOTTY: 25 Inappropriate ioctl for device. */
		return -ENOTTY;
        }

        return 0;
}

static int vl53l0x_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * Specifying the .owner ensures that a reference is taken during any calls to
 * the function pointers in this table.
 *
 * This prevents unloading this module while an application is reading from
 * the device for example.
 *
 * Removing the .owner would allow userspace to remove the module while it's in
 * active use, leading towards a kernel panic.
 */
static const struct file_operations vl53l0x_fops = {
	.owner		= THIS_MODULE,
	.open		= vl53l0x_open,
	.read  		= vl53l0x_read,
	.unlocked_ioctl	= vl53l0x_ioctl,
	.release	= vl53l0x_release,
	.llseek		= no_llseek,
};

static int vl53l0x_probe(struct i2c_client *client)
{
	struct vl53l0x *vl53l0x;
	int ret;

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

	/*
	 * A 'proximity' framework should be responsible for creating the class.
	 * We do not have such a framework, so we create the class ourselves.
	 */
	proximity.class = class_create(THIS_MODULE, "proximity");
	if (IS_ERR(proximity.class))
		return PTR_ERR(proximity.class);

	ret = register_chrdev(0, "proximity", &vl53l0x_fops);
	if (ret < 0)
		return ret;

	/* Successfully registered */
	proximity.major = ret;

	printk(KERN_INFO "Proximity class created with major number %d.\n", proximity.major);

	/* Create only a single proximity device with minor 0. */
	vl53l0x->minor = 0;

	/* Create the device file for userspace to interact with. */
	vl53l0x->dev = device_create(proximity.class, NULL,
				     MKDEV(proximity.major, vl53l0x->minor),
				     vl53l0x, "proximity%d", vl53l0x->minor);

	/* Register the device in the proximity device list. */
	REGISTER_PROXIMITY(vl53l0x->minor, vl53l0x);

	/* Setup debugfs data directory & file */
	proximity_data = debugfs_create_dir("vl53l0x", NULL);
	proximity_rate = debugfs_create_u32("refresh_rate", 0444, proximity_data, &vl53l0x->rate);

	return 0;
}

static int vl53l0x_remove(struct i2c_client *client)
{
	struct vl53l0x *vl53l0x = i2c_get_clientdata(client);

	device_del(vl53l0x->dev);
	kfree(vl53l0x);

	/* Framework cleanup. */
	unregister_chrdev(proximity.major, "proximity");
	class_destroy(proximity.class);

	/* debugfs cleanup*/
    debugfs_remove_recursive(proximity_data);

	return 0;
}

static const struct i2c_device_id vl53l0x_id[] = {
	{ "vl53l0x", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, vl53l0x_id);

static const struct of_device_id vl53l0x_dt_match[] = {
	{ .compatible = "st,vl53l0x", },
	{ }
};
MODULE_DEVICE_TABLE(of, vl53l0x_dt_match);

static struct i2c_driver vl53l0x_driver = {
	.driver = {
		.name = "vl53l0x",
		.of_match_table = vl53l0x_dt_match,
	},
	.probe_new = vl53l0x_probe,
	.remove = vl53l0x_remove,
	.id_table = vl53l0x_id,
};
module_i2c_driver(vl53l0x_driver);

MODULE_DESCRIPTION("VL53L0X I2C Time of Flight driver");
MODULE_AUTHOR("Kieran Bingham <kieran@linuxembedded.co.uk>");
MODULE_LICENSE("GPL");
