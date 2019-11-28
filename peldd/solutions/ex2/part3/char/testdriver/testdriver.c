/* The traditional Hello World module */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

static int value = 0;
static char *string = "Linux";

module_param(value, int, 0644);
module_param(string, charp, 0644);

MODULE_PARM_DESC(value, "an integer test value");

int __init simple_init(void)
{
    pr_info("hello world! value is: %d\n", value);

    return 0;
}

void __exit simple_exit(void)
{
    pr_info("bye bye, string is: %s\n", string);
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
