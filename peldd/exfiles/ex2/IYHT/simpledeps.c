/* The traditional Hello World module */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

void myprint(void)
{
	pr_info("This is an exported function\n");
}

EXPORT_SYMBOL(myprint);

int __init simple_init(void)
{
    pr_info("hello world!\n");
    return 0;
}

void __exit simple_exit(void)
{
    pr_info("bye bye\n");
}

module_init(simple_init);
module_exit(simple_exit);

static const struct file_operations simple_fops = {
	.owner	= THIS_MODULE,
};

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("module author <module@author.com>");
MODULE_DESCRIPTION("Test module");
MODULE_VERSION("1.0");
