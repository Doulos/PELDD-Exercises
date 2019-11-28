#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/device.h>

static void *kptr;
static size_t total = 0;
static size_t mbsize, size;
static u32 alloc_count = 0;


static ssize_t miscmem_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	size = mbsize * 1024 * 1024;

        pr_info("read /dev/miscmem to trigger memory allocation of size %ld bytes\n", size);

	/* Allocate 'size' bytes here */
	
	if (!kptr)
		return -ENOMEM;

	total += size;
	alloc_count++;

	pr_info("total allocation from %d allocs is %ld bytes (%ld MB)\n", alloc_count, total, (total / (1024 * 1024)));

	return count;
}

static ssize_t miscmem_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	char temp[32];
	int ret;

	if (copy_from_user(temp, buf, count) != 0)
		return -EFAULT;

	ret = sscanf(temp, "%ld", &mbsize);

	pr_info("Alloc size is %ld MB\n", mbsize);
	
	return count;
}

static const struct file_operations miscmem_fops = {
        .owner  = THIS_MODULE,
	.read	= miscmem_read,
	.write	= miscmem_write,
};

static struct miscdevice miscmem =
{
	MISC_DYNAMIC_MINOR,
	"miscmem",
	&miscmem_fops
};

int __init miscmem_init(void)
{
	misc_register(&miscmem);
    	pr_info("Init miscmem\n");
    	return 0;
}

void __exit miscmem_exit(void)
{
	misc_deregister(&miscmem);
	pr_info("Exit miscmem\n");
}

module_init(miscmem_init);
module_exit(miscmem_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
