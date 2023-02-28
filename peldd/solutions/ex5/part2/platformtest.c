#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fwnode.h>

static int pdev_probe(struct platform_device *drv){
    struct resource *res1;
    struct device *dev = &drv->dev;

    /* new variables for node handling */
    int ret = 0;
    u32 value = 0;
    const char *string = NULL;

    /* derive fw node handle from device structure */
    struct fwnode_handle *fwnode = dev_fwnode(dev);

    /* check valid fwnode */
    /* return 'no such device or address' if not */
    if (!fwnode)
           return -ENXIO;

    dev_info(dev, "platform_device->name %s\n", drv->name);

    res1 = platform_get_resource(drv, IORESOURCE_MEM, 0);

    dev_info(dev, "resource start: 0x%8x\n", (int)res1->start);
    dev_info(dev, "resource end: 0x%8x\n", (int)res1->end);
    dev_info(dev, "resource name: %s\n", res1->name);

    ret = fwnode_property_read_string(fwnode, "mystring", &string);

    if (ret) {
	dev_err(dev, "mystring not defined: %d\n", ret);
	return ret;
    }

    ret = fwnode_property_read_u32(fwnode, "myvalue", &value);

    if (ret) {
	dev_err(dev, "myvalue not defined: %d\n", ret);
	return ret;
    }

    if(fwnode_property_read_bool(fwnode, "mybool")) {
	dev_info(dev, "mystring is: %s\n", string);
	dev_info(dev, "myvalue is: %d\n", value);
    } else {
	dev_err(dev, "mybool not defined\n");
	return -ENXIO;
    }


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
