// SPDX-License-Identifier: GPL-2.0+

/*
 * Character LCD driver for Linux
 *
 * Copyright (C) 2000-2008, Willy Tarreau <w@1wt.eu>
 * Copyright (C) 2016-2017 Glider bvba
 *
 * Grove LCD Character driver for Linux.
 *
 * Copyright (C) 2019 Kieran Bingham <kieran@linuxembedded.co.uk>
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>

#include <linux/i2c.h>

#include <linux/atomic.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <generated/utsrelease.h>

#define LCD_MINOR		156

#define DEFAULT_LCD_BWIDTH      40
#define DEFAULT_LCD_HWIDTH      64

#define LCD_FLAG_B		0x0004	/* Blink on */
#define LCD_FLAG_C		0x0008	/* Cursor on */
#define LCD_FLAG_D		0x0010	/* Display on */
#define LCD_FLAG_F		0x0020	/* Large font mode */
#define LCD_FLAG_N		0x0040	/* 2-rows mode */

/* LCD commands */
#define LCD_CMD_DISPLAY_CLEAR	0x01	/* Clear entire display */

#define LCD_CMD_ENTRY_MODE	0x04	/* Set entry mode */
#define LCD_CMD_CURSOR_INC	0x02	/* Increment cursor */

#define LCD_CMD_DISPLAY_CTRL	0x08	/* Display control */
#define LCD_CMD_DISPLAY_ON	0x04	/* Set display on */
#define LCD_CMD_CURSOR_ON	0x02	/* Set cursor on */
#define LCD_CMD_BLINK_ON	0x01	/* Set blink on */

#define LCD_CMD_SHIFT		0x10	/* Shift cursor/display */
#define LCD_CMD_DISPLAY_SHIFT	0x08	/* Shift display instead of cursor */
#define LCD_CMD_SHIFT_RIGHT	0x04	/* Shift display/cursor to the right */

#define LCD_CMD_FUNCTION_SET	0x20	/* Set function */
#define LCD_CMD_DATA_LEN_8BITS	0x10	/* Set data length to 8 bits */
#define LCD_CMD_TWO_LINES	0x08	/* Set to two display lines */
#define LCD_CMD_FONT_5X10_DOTS	0x04	/* Set char font to 5x10 dots */

#define LCD_CMD_SET_CGRAM_ADDR	0x40	/* Set char generator RAM address */

#define LCD_CMD_SET_DDRAM_ADDR	0x80	/* Set display data RAM address */


#define LCD_MESSAGE_HISTORY	10	/* Store these messages in the driver */

struct charlcd_msg {
	unsigned int length;
	char *message;

	/* The list entry allocation. For the list API usage see:
	 * https://www.kernel.org/doc/htmldocs/kernel-api/adt.html#id-1.3.2 */
	struct list_head list;
};

struct charlcd {
	const struct charlcd_ops *ops;
	const unsigned char *char_conv;	/* Optional */
	struct mutex lock;		/* Serialise writes to the display */

	int ifwidth;			/* 4-bit or 8-bit (default) */
	int height;
	int width;
	int bwidth;			/* Default set by charlcd_alloc() */
	int hwidth;			/* Default set by charlcd_alloc() */

	void *drvdata;			/* Set by charlcd_alloc() */

	struct list_head list;		/* Message history list */
	unsigned int history_len;	/* Message history size */
};

struct charlcd_ops {
	/* Required */
	void (*write_cmd)(struct charlcd *lcd, int cmd);
	void (*write_data)(struct charlcd *lcd, int data);

	/* Optional */
	void (*clear_fast)(struct charlcd *lcd);
};

struct charlcd_priv {
	struct charlcd lcd;

	bool must_clear;

	/* contains the LCD config state */
	unsigned long int flags;

	/* Contains the LCD X and Y offset */
	struct {
		unsigned long int x;
		unsigned long int y;
	} addr;

	unsigned long long drvdata[0];
};

#define charlcd_to_priv(p)	container_of(p, struct charlcd_priv, lcd)

/* sleeps that many milliseconds with a reschedule */
static void long_sleep(int ms)
{
	schedule_timeout_interruptible(msecs_to_jiffies(ms));
}

static void charlcd_gotoxy(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);
	unsigned int addr;

	/*
	 * we force the cursor to stay at the end of the
	 * line if it wants to go farther
	 */
	addr = priv->addr.x < lcd->bwidth ? priv->addr.x & (lcd->hwidth - 1)
					  : lcd->bwidth - 1;
	if (priv->addr.y & 1)
		addr += lcd->hwidth;
	if (priv->addr.y & 2)
		addr += lcd->bwidth;
	lcd->ops->write_cmd(lcd, LCD_CMD_SET_DDRAM_ADDR | addr);
}

static void charlcd_home(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	priv->addr.x = 0;
	priv->addr.y = 0;
	charlcd_gotoxy(lcd);
}

static void charlcd_print(struct charlcd *lcd, char c)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	/* Sleep between each character to allow the display to update */
	usleep_range(5000, 10000);

	if (priv->addr.x < lcd->bwidth) {
		if (lcd->char_conv)
			c = lcd->char_conv[(unsigned char)c];
		lcd->ops->write_data(lcd, c);
		priv->addr.x++;

		/* prevents the cursor from wrapping onto the next line */
		if (priv->addr.x == lcd->bwidth)
			charlcd_gotoxy(lcd);
	}
}

static void charlcd_clear_fast(struct charlcd *lcd)
{
	int pos;

	charlcd_home(lcd);

	if (lcd->ops->clear_fast)
		lcd->ops->clear_fast(lcd);
	else
		for (pos = 0; pos < min(2, lcd->height) * lcd->hwidth; pos++)
			lcd->ops->write_data(lcd, ' ');

	charlcd_home(lcd);
}

/* clears the display and resets X/Y */
static void charlcd_clear_display(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	lcd->ops->write_cmd(lcd, LCD_CMD_DISPLAY_CLEAR);
	priv->addr.x = 0;
	priv->addr.y = 0;
	/* we must wait a few milliseconds (15) */
	long_sleep(15);
}

static int charlcd_init_display(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);
	u8 init;

	priv->flags = ((lcd->height > 1) ? LCD_FLAG_N : 0) | LCD_FLAG_D;

	long_sleep(20);		/* wait 20 ms after power-up for the paranoid */

	/*
	 * 8-bit mode, 1 line, small fonts; let's do it 3 times, to make sure
	 * the LCD is in 8-bit mode afterwards
	 */
	init = LCD_CMD_FUNCTION_SET | LCD_CMD_DATA_LEN_8BITS;

	lcd->ops->write_cmd(lcd, init);
	long_sleep(10);
	lcd->ops->write_cmd(lcd, init);
	long_sleep(10);
	lcd->ops->write_cmd(lcd, init);
	long_sleep(10);

	/* set font height and lines number */
	lcd->ops->write_cmd(lcd,
		LCD_CMD_FUNCTION_SET | LCD_CMD_DATA_LEN_8BITS |
		((priv->flags & LCD_FLAG_F) ? LCD_CMD_FONT_5X10_DOTS : 0) |
		((priv->flags & LCD_FLAG_N) ? LCD_CMD_TWO_LINES : 0));
	long_sleep(10);

	/* display off, cursor off, blink off */
	lcd->ops->write_cmd(lcd, LCD_CMD_DISPLAY_CTRL);
	long_sleep(10);

	lcd->ops->write_cmd(lcd,
		LCD_CMD_DISPLAY_CTRL |	/* set display mode */
		((priv->flags & LCD_FLAG_D) ? LCD_CMD_DISPLAY_ON : 0) |
		((priv->flags & LCD_FLAG_C) ? LCD_CMD_CURSOR_ON : 0) |
		((priv->flags & LCD_FLAG_B) ? LCD_CMD_BLINK_ON : 0));

	long_sleep(10);

	/* entry mode set : increment, cursor shifting */
	lcd->ops->write_cmd(lcd, LCD_CMD_ENTRY_MODE | LCD_CMD_CURSOR_INC);

	charlcd_clear_display(lcd);
	return 0;
}

/*
 * These are the file operation function for user access to /dev/lcd
 * This function can also be called from inside the kernel, by
 * setting file and ppos to NULL.
 *
 */

static void charlcd_write_char(struct charlcd *lcd, char c)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);

	switch (c) {
	case '\b':
		/* go back one char and clear it */
		if (priv->addr.x > 0) {
			/*
			 * check if we're not at the
			 * end of the line
			 */
			if (priv->addr.x < lcd->bwidth)
				/* back one char */
				lcd->ops->write_cmd(lcd, LCD_CMD_SHIFT);
			priv->addr.x--;
		}
		/* replace with a space */
		lcd->ops->write_data(lcd, ' ');
		/* back one char again */
		lcd->ops->write_cmd(lcd, LCD_CMD_SHIFT);
		break;
	case '\f':
		/* quickly clear the display */
		charlcd_clear_fast(lcd);
		break;
	case '\n':
		/*
		 * flush the remainder of the current line and
		 * go to the beginning of the next line
		 */
		for (; priv->addr.x < lcd->bwidth; priv->addr.x++)
			lcd->ops->write_data(lcd, ' ');
		priv->addr.x = 0;
		priv->addr.y = (priv->addr.y + 1) % lcd->height;
		charlcd_gotoxy(lcd);
		break;
	case '\r':
		/* go to the beginning of the same line */
		priv->addr.x = 0;
		charlcd_gotoxy(lcd);
		break;
	case '\t':
		/* print a space instead of the tab */
		charlcd_print(lcd, ' ');
		break;
	default:
		/* simply print this char */
		charlcd_print(lcd, c);
		break;
	}
}

static struct charlcd *the_charlcd;

static ssize_t charlcd_write_msg(struct charlcd *lcd, struct charlcd_msg *msg)
{
	unsigned int i;

	/* Prevent screen corruption of multiple writes.
	 * Whole messages must complete without interruption. */
	mutex_lock(&lcd->lock);

	for (i = 0; i < msg->length; i++) {
		charlcd_write_char(lcd, msg->message[i]);
	}

	mutex_unlock(&the_charlcd->lock);

	return i;
}

/* Read in the user buffer and store it in our message history buffer
 * then, write the message to the LCD.
 */
static ssize_t charlcd_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	int ret;

	/* To retain a history of messages, duplicate the message and store it
	 * in a list for future processing.
	 */
	struct charlcd_msg *msg = kmalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	/* Allocate message storage to duplicate the string.
	 * We can't use kstrdup here, because the characters must be obtained
	 * from userspace through the uaccess helpers
	 */
	msg->message = kmalloc(count + 1, GFP_KERNEL);
	if (!msg->message)
		return -ENOMEM;

	msg->length = count;

	/* Ensure that the strings are null terminated */
	msg->message[count] = '\0';

	/* Copy the string from userspace to our kernel history cache */
	ret = strncpy_from_user(msg->message, buf, count);
	if (ret < 0)
		return ret;

	/* Increase the file position pointer by the amount of data we have consumed. */
	ppos += ret;

	/* Add this new entry to the end of our message history list */
	list_add_tail(&msg->list, &the_charlcd->list);

	if (the_charlcd->history_len < LCD_MESSAGE_HISTORY) {
		the_charlcd->history_len++;
	} else {
		/* Find the oldest message in the list so we can remove it */
		struct charlcd_msg *old = list_first_entry(&the_charlcd->list,
							   struct charlcd_msg,
							   list);

		printk("Removing entry of length %d : %s\n", old->length, old->message);

		/* Remove the old entry from the list. */
		list_del_init(&old->list);

		/* Release allocations */
		kfree(old->message);
		kfree(old);
	}

	return charlcd_write_msg(the_charlcd, msg);
}

static int charlcd_open(struct inode *inode, struct file *file)
{
	struct charlcd_priv *priv = charlcd_to_priv(the_charlcd);
	int ret;

	ret = -EPERM;
	if (file->f_mode & FMODE_READ)	/* device is write-only */
		goto fail;

	if (priv->must_clear) {
		charlcd_clear_display(&priv->lcd);
		priv->must_clear = false;
	}
	return nonseekable_open(inode, file);

 fail:
	return ret;
}

static int charlcd_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations charlcd_fops = {
	.write   = charlcd_write,
	.open    = charlcd_open,
	.release = charlcd_release,
	.llseek  = no_llseek,
};

static struct miscdevice charlcd_dev = {
	.minor	= LCD_MINOR,
	.name	= "lcd",
	.fops	= &charlcd_fops,
};

static void charlcd_puts(struct charlcd *lcd, const char *s)
{
	const char *tmp = s;
	int count = strlen(s);

	mutex_lock(&lcd->lock);

	for (; count-- > 0; tmp++) {
		charlcd_write_char(lcd, *tmp);
	}

	mutex_unlock(&lcd->lock);
}

#ifdef CONFIG_PANEL_BOOT_MESSAGE
#define LCD_INIT_TEXT CONFIG_PANEL_BOOT_MESSAGE
#else
//#define LCD_INIT_TEXT "Linux-" UTS_RELEASE "\n"
#define LCD_INIT_TEXT __DATE__ "\n" __TIME__ "\n"
#endif

/* initialize the LCD driver */
static int charlcd_init(struct charlcd *lcd)
{
	struct charlcd_priv *priv = charlcd_to_priv(lcd);
	int ret;

	/*
	 * before this line, we must NOT send anything to the display.
	 * Since charlcd_init_display() needs to write data, we have to
	 * enable mark the LCD initialized just before.
	 */
	ret = charlcd_init_display(lcd);
	if (ret)
		return ret;

	/* display a short message */
	charlcd_puts(lcd, "\f" LCD_INIT_TEXT);

	/* clear the display on the next device opening */
	priv->must_clear = true;
	charlcd_home(lcd);
	return 0;
}

static struct charlcd *charlcd_alloc(unsigned int drvdata_size)
{
	struct charlcd_priv *priv;
	struct charlcd *lcd;

	priv = kzalloc(sizeof(*priv) + drvdata_size, GFP_KERNEL);
	if (!priv)
		return NULL;

	lcd = &priv->lcd;
	lcd->bwidth = DEFAULT_LCD_BWIDTH;
	lcd->hwidth = DEFAULT_LCD_HWIDTH;
	lcd->drvdata = priv->drvdata;

	/* The list structure must be initialised before it is used. */
	INIT_LIST_HEAD(&lcd->list);

	return lcd;
}

static void charlcd_free(struct charlcd *lcd)
{
	struct charlcd_msg *msg;

	/* Iterate through every member of list */
	list_for_each_entry(msg, &lcd->list, list) {
		printk("Msg Len %d : %s\n", msg->length, msg->message);

		kfree(msg->message);
		kfree(msg);
	}

	kfree(charlcd_to_priv(lcd));
}

static int panel_notify_sys(struct notifier_block *this, unsigned long code,
			    void *unused)
{
	struct charlcd *lcd = the_charlcd;

	switch (code) {
	case SYS_DOWN:
		charlcd_puts(lcd, "\fReloading\nSystem...");
		break;
	case SYS_HALT:
		charlcd_puts(lcd, "\fSystem Halted.");
		break;
	case SYS_POWER_OFF:
		charlcd_puts(lcd, "\fPower off.");
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block panel_notifier = {
	panel_notify_sys,
	NULL,
	0
};

static int charlcd_register(struct charlcd *lcd)
{
	int ret;

	ret = charlcd_init(lcd);
	if (ret)
		return ret;

	ret = misc_register(&charlcd_dev);
	if (ret)
		return ret;

	the_charlcd = lcd;
	register_reboot_notifier(&panel_notifier);
	return 0;
}

static int charlcd_unregister(struct charlcd *lcd)
{
	unregister_reboot_notifier(&panel_notifier);
	charlcd_puts(lcd, "\fLCD driver unloaded.");
	misc_deregister(&charlcd_dev);
	the_charlcd = NULL;

	return 0;
}

/*******************************************************************************
 *      		Internal Print function				       *
 *									       *
 * This utilises the static global singleton reference to the charlcd instance *
 * This is not a recommended means of communicating between modules.	       *
 *									       *
 ******************************************************************************/
void charlcd_printstr(const char *s);

void charlcd_printstr(const char *s)
{
	charlcd_puts(the_charlcd, s);
}
EXPORT_SYMBOL_GPL(charlcd_printstr);

/*******************************************************************************
 *									       *
 *         JHD1313 - Support for the Grove-LCD RGB Backlight Display	       *
 *									       *
 * This supports the LCD Display only. The Backlight is driven through the     *
 * PCA9633 driver and the LED framework.				       *
 *									       *
 ******************************************************************************/

struct jhd1313 {
	struct i2c_client *client;
};

static void jhd1313_write_cmd(struct charlcd *lcd, int cmd)
{
	struct jhd1313 *jhd = lcd->drvdata;
	struct i2c_client *client = jhd->client;

	i2c_smbus_write_byte_data(client, 0x00, cmd);
}

static void jhd1313_write_data(struct charlcd *lcd, int data)
{
	struct jhd1313 *jhd = lcd->drvdata;
	struct i2c_client *client = jhd->client;

	i2c_smbus_write_byte_data(client, 0x40, data);
}

static const struct charlcd_ops jhd1313_ops = {
	.write_cmd	= jhd1313_write_cmd,
	.write_data	= jhd1313_write_data,
};

static int jhd1313_probe(struct i2c_client *client)
{
	struct charlcd *lcd;
	struct jhd1313 *jhd;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

	lcd = charlcd_alloc(sizeof(struct jhd1313));
	if (!lcd)
		return -ENOMEM;

	jhd = lcd->drvdata;
	i2c_set_clientdata(client, lcd);
	jhd->client = client;

	lcd->width = 16;
	lcd->height = 2;
	lcd->ifwidth = 8;
	lcd->ops = &jhd1313_ops;

	mutex_init(&lcd->lock);

	ret = charlcd_register(lcd);
	if (ret) {
		charlcd_free(lcd);
		dev_err(&client->dev, "Failed to register JHD1313");
	}

	return ret;
}

static int jhd1313_remove(struct i2c_client *client)
{
	struct charlcd *lcd = i2c_get_clientdata(client);

	charlcd_unregister(lcd);
	charlcd_free(lcd);

	return 0;
}

static const struct i2c_device_id jhd1313_id[] = {
	{ "jhd1313", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, jhd1313_id);

static const struct of_device_id jhd1313_of_table[] = {
	{ .compatible = "jhd,jhd1313" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jhd1313_of_table);

static struct i2c_driver jhd1313_driver = {
	.driver = {
		.name = "jhd1313",
		.of_match_table = jhd1313_of_table,
	},
	.probe_new = jhd1313_probe,
	.remove = jhd1313_remove,
	.id_table = jhd1313_id,
};

module_i2c_driver(jhd1313_driver);

MODULE_DESCRIPTION("JHD1313 I2C Character LCD driver");
MODULE_AUTHOR("Kieran Bingham <kieran@linuxembedded.co.uk>");
MODULE_LICENSE("GPL");
