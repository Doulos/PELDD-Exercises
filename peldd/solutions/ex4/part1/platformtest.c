#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>

static int pdev_probe(struct platform_device *drv){
    struct resource *res1;
    struct device *dev = &drv->dev;

    dev_info(dev, "platform_device->name %s\n", drv->name);

    res1 = platform_get_resource(drv, IORESOURCE_MEM, 0);

    dev_info(dev, "resource start: 0x%8x\n", (int)res1->start);
    dev_info(dev, "resource end: 0x%8x\n", (int)res1->end);
    dev_info(dev, "resource name: %s\n", res1->name);

    return 0;
}

static int pdev_remove(struct platform_device *drv){
    dev_info(&drv->dev, "test device removed\n");
    return 0;
}

static struct of_device_id pdev_ids[] = {
        { .compatible = "test,pdev_test" },
        { /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, pdev_ids);

static struct platform_driver pdev_test_driver = {
         .probe = pdev_probe,
        .remove = pdev_remove,
        .driver = {
                .name = "pdev_test_driver",
                .of_match_table = of_match_ptr(pdev_ids),
                .owner = THIS_MODULE,
        },
};

module_platform_driver(pdev_test_driver);

MODULE_LICENSE("GPL");
