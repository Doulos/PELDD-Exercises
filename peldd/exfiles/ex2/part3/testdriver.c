/* The traditional Hello World module */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

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

