/* driver/misc/sunxi-reg.c
 *
 *  Copyright (C) 2011 Reuuimlla Technology Co.Ltd
 *  Charles <yanjianbo@allwinnertech.com>
 *
 *  www.reuuimllatech.com
 *
 *  User access to the registers driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/sysfs.h>

#include <linux/sunxi-gpio.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include "../base/base.h"

#define PINS_PER_BANK       32

DECLARE_RWSEM(gpio_sw_list_lock);
LIST_HEAD(gpio_sw_list);
static struct class *gpio_sw_class;

struct sw_gpio_pd {
	char name[16];
	char link[16];
	unsigned int light;
};

struct gpio_sw_classdev {
	const char *name;
	unsigned int pull, drv, cfg;
	struct mutex class_mutex;
	struct gpio_config *item;
	int (*gpio_sw_cfg_set) (struct gpio_sw_classdev *gpio_sw_cdev,
				int mul_cfg);
	int (*gpio_sw_pull_set) (struct gpio_sw_classdev *gpio_sw_cdev,
				 int mul_cfg);
	int (*gpio_sw_data_set) (struct gpio_sw_classdev *gpio_sw_cdev,
				 int mul_cfg);
	int (*gpio_sw_drv_set) (struct gpio_sw_classdev *gpio_sw_cdev,
				int mul_cfg);
	int (*gpio_sw_cfg_get) (struct gpio_sw_classdev *gpio_sw_cdev);
	int (*gpio_sw_pull_get) (struct gpio_sw_classdev *gpio_sw_cdev);
	int (*gpio_sw_data_get) (struct gpio_sw_classdev *gpio_sw_cdev);
	int (*gpio_sw_drv_get) (struct gpio_sw_classdev *gpio_sw_cdev);
	struct device *dev;
	struct list_head node;
};

struct sw_gpio {
	struct sw_gpio_pd *pdata;
	spinlock_t lock;
	struct gpio_sw_classdev class;
};

static struct platform_device *gpio_sw_dev[256];
static struct sw_gpio_pd *sw_pdata[256];
struct device_node *node;
static unsigned int easy_light_used;

/*
 * mul_cfg: 0 - inpit
 *          1 - output
*/
static int gpio_sw_cfg_set(struct gpio_sw_classdev *gpio_sw_cdev, int mul_cfg)
{
	char pin_name[32];
	unsigned long config;

	if (mul_cfg == 0)
		gpio_direction_input(gpio_sw_cdev->item->gpio);
	else if (mul_cfg == 1)
		gpio_direction_output(gpio_sw_cdev->item->gpio, 0);
	else if (mul_cfg > 1 && mul_cfg <= 7) {
		sunxi_gpio_to_name(gpio_sw_cdev->item->gpio, pin_name);
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, mul_cfg);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
	}
	gpio_sw_cdev->cfg = mul_cfg;
	return 0;
}

static int gpio_sw_cfg_get(struct gpio_sw_classdev *gpio_sw_cdev)
{
	return 0;
}

static int gpio_sw_pull_set(struct gpio_sw_classdev *gpio_sw_cdev, int pull)
{
	char pin_name[32];
	unsigned long config;

	if (pull >= 0 && pull <= 3) {
		sunxi_gpio_to_name(gpio_sw_cdev->item->gpio, pin_name);
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, pull);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
	}

	gpio_sw_cdev->pull = pull;
	return 0;
}

static int gpio_sw_pull_get(struct gpio_sw_classdev *gpio_sw_cdev)
{

	return 0;
}

static int gpio_sw_drv_set(struct gpio_sw_classdev *gpio_sw_cdev, int drv)
{
	char pin_name[32];
	unsigned long config;

	if (drv >= 0 && drv <= 3) {
		sunxi_gpio_to_name(gpio_sw_cdev->item->gpio, pin_name);
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, drv);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
	}

	gpio_sw_cdev->drv = drv;
	return 0;
}

static int gpio_sw_drv_get(struct gpio_sw_classdev *gpio_sw_cdev)
{

	return 0;
}

static int gpio_sw_data_set(struct gpio_sw_classdev *gpio_sw_cdev, int data)
{
	__gpio_set_value(gpio_sw_cdev->item->gpio, data);
	return 0;
}

static int gpio_sw_data_get(struct gpio_sw_classdev *gpio_sw_cdev)
{
	return __gpio_get_value(gpio_sw_cdev->item->gpio);
}

static ssize_t cfg_sel_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	int length;

	mutex_lock(&gpio_sw_cdev->class_mutex);
	gpio_sw_cdev->item->mul_sel =
	    gpio_sw_cdev->gpio_sw_drv_get(gpio_sw_cdev);
	length = sprintf(buf, "%u\n", gpio_sw_cdev->cfg);
	mutex_unlock(&gpio_sw_cdev->class_mutex);

	return length;
}

static ssize_t pull_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	int length;

	mutex_lock(&gpio_sw_cdev->class_mutex);
	gpio_sw_cdev->item->pull = gpio_sw_cdev->gpio_sw_drv_get(gpio_sw_cdev);
	length = sprintf(buf, "%u\n", gpio_sw_cdev->pull);
	mutex_unlock(&gpio_sw_cdev->class_mutex);

	return length;
}

static ssize_t drv_level_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	int length;

	mutex_lock(&gpio_sw_cdev->class_mutex);
	gpio_sw_cdev->item->drv_level =
	    gpio_sw_cdev->gpio_sw_drv_get(gpio_sw_cdev);
	length = sprintf(buf, "%u\n", gpio_sw_cdev->drv);
	mutex_unlock(&gpio_sw_cdev->class_mutex);

	return length;
}

static ssize_t data_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	int length;

	mutex_lock(&gpio_sw_cdev->class_mutex);
	gpio_sw_cdev->item->data = gpio_sw_cdev->gpio_sw_data_get(gpio_sw_cdev);
	length = sprintf(buf, "%u\n", gpio_sw_cdev->item->data);
	mutex_unlock(&gpio_sw_cdev->class_mutex);

	return length;
}

static ssize_t cfg_sel_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t size)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	unsigned long cfg;

	if (kstrtoul(buf, 10, &cfg))
		return -EINVAL;
	if (cfg > 7) {
		return size;
	}
	gpio_sw_cdev->gpio_sw_cfg_set(gpio_sw_cdev, cfg);
	return size;
}

static ssize_t pull_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t size)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	unsigned long pull;

	if (kstrtoul(buf, 10, &pull))
		return -EINVAL;

	if (pull > 3) {
		return size;
	}

	gpio_sw_cdev->gpio_sw_pull_set(gpio_sw_cdev, pull);
	return size;
}

static ssize_t drv_level_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t size)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);

	unsigned long drv_level;
	if (kstrtoul(buf, 10, &drv_level))
		return -EINVAL;

	if (drv_level > 3) {
		return size;
	}

	gpio_sw_cdev->gpio_sw_drv_set(gpio_sw_cdev, drv_level);
	return size;
}

static ssize_t data_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t size)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	unsigned long data;

	if (kstrtoul(buf, 10, &data))
		return -EINVAL;

	gpio_sw_cdev->gpio_sw_data_set(gpio_sw_cdev, data);
	return size;
}

static ssize_t light_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t size)
{
	struct gpio_sw_classdev *gpio_sw_cdev = dev_get_drvdata(dev);
	struct sw_gpio_pd *pdata = dev->parent->platform_data;
	unsigned long data;

	if (kstrtoul(buf, 10, &data))
		return -EINVAL;
	if (data)
		gpio_sw_cdev->gpio_sw_data_set(gpio_sw_cdev, pdata->light ? 1 : 0);
	else
		gpio_sw_cdev->gpio_sw_data_set(gpio_sw_cdev, pdata->light ? 0 : 1);

	return size;
}

static int gpio_sw_suspend(struct device *dev, pm_message_t state)
{
	return 0;
}

static int gpio_sw_resume(struct device *dev)
{
	return 0;
}

static struct device_attribute gpio_sw_class_attrs[] = {
	__ATTR(cfg, 0664, cfg_sel_show, cfg_sel_store),
	__ATTR(pull, 0664, pull_show, pull_store),
	__ATTR(drv, 0664, drv_level_show, drv_level_store),
	__ATTR(data, 0664, data_show, data_store),
	__ATTR_NULL,
};

static struct device_attribute easy_light_attr =
	__ATTR(light, 0664, data_show, light_store);

void gpio_sw_classdev_unregister(struct gpio_sw_classdev *gpio_sw_cdev)
{
	mutex_destroy(&gpio_sw_cdev->class_mutex);
	device_unregister(gpio_sw_cdev->dev);
	down_write(&gpio_sw_list_lock);
	list_del(&gpio_sw_cdev->node);
	up_write(&gpio_sw_list_lock);
}

static int gpio_sw_remove(struct platform_device *dev)
{
	struct sw_gpio *sw_gpio_entry = platform_get_drvdata(dev);
	struct sw_gpio_pd *pdata = dev->dev.platform_data;

	if (strlen(pdata->link) != 0)
		sysfs_remove_link(&gpio_sw_class->p->subsys.kobj, pdata->link);

	gpio_sw_classdev_unregister(&sw_gpio_entry->class);
	kfree(sw_gpio_entry->class.item);
	kfree(sw_gpio_entry);
	return 0;
}

int
gpio_sw_classdev_register(struct device *parent,
			  struct gpio_sw_classdev *gpio_sw_cdev)
{
	struct sw_gpio_pd *pdata = parent->platform_data;
	int i = 0;

	gpio_sw_cdev->dev = device_create(gpio_sw_class, parent, 0,
					  gpio_sw_cdev, "%s",
					  gpio_sw_cdev->name);
	for (i = 0; i < (sizeof(gpio_sw_class_attrs)/sizeof(struct device_attribute) - 1); i++) {
		sysfs_add_file_to_group(&gpio_sw_cdev->dev->kobj, &(gpio_sw_class_attrs[i].attr), NULL);
	}
	if (IS_ERR(gpio_sw_cdev->dev))
		return PTR_ERR(gpio_sw_cdev->dev);
	if (easy_light_used && strlen(pdata->link)) {
		if (sysfs_create_file(&gpio_sw_cdev->dev->kobj, &easy_light_attr.attr))
			pr_err("gpio_sw: sysfs_create_file fail\n");
	}
	down_write(&gpio_sw_list_lock);
	list_add_tail(&gpio_sw_cdev->node, &gpio_sw_list);
	up_write(&gpio_sw_list_lock);
	mutex_init(&gpio_sw_cdev->class_mutex);

	return 0;
}

static int map_gpio_to_name(char *name, u32 gpio)
{
	char base;
	int num;
	num = gpio - SUNXI_PA_BASE;
	if (num < 0)
		goto map_fail;

	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'A';
		goto map_done;
	}
	num = gpio - SUNXI_PB_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'B';
		goto map_done;
	}
	num = gpio - SUNXI_PC_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'C';
		goto map_done;
	}
	num = gpio - SUNXI_PD_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'D';
		goto map_done;
	}
	num = gpio - SUNXI_PE_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'E';
		goto map_done;
	}
	num = gpio - SUNXI_PF_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'F';
		goto map_done;
	}
	num = gpio - SUNXI_PG_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'G';
		goto map_done;
	}
	num = gpio - SUNXI_PH_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'H';
		goto map_done;
	}
	num = gpio - SUNXI_PJ_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'J';
		goto map_done;
	}
	num = gpio - SUNXI_PL_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'L';
		goto map_done;
	}
	num = gpio - SUNXI_PM_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'M';
		goto map_done;
	}
	num = gpio - AXP_PIN_BASE;
	if ((num >= 0) && (num < PINS_PER_BANK)) {
		base = 'X';
		goto map_done;
	}
	goto map_fail;
map_done:
	sprintf(name, "P%c%d", base, num);
	return 0;
map_fail:
	return -1;
}

static void gpio_sw_release(struct device *dev)
{
	pr_info("gpio_sw_release good !\n");
}

static int gpio_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int gpio_resume(struct platform_device *dev)
{
	return 0;
}

static int gpio_sw_probe(struct platform_device *dev)
{
	struct sw_gpio *sw_gpio_entry;
	struct sw_gpio_pd *pdata = dev->dev.platform_data;
	int ret;
	unsigned long flags;
	char io_area[16];
	int gpio;

	sw_gpio_entry = kzalloc(sizeof(struct sw_gpio), GFP_KERNEL);
	if (!sw_gpio_entry)
		return -ENOMEM;

	sw_gpio_entry->class.item =
	    kzalloc(sizeof(struct gpio_config), GFP_KERNEL);
	if (!sw_gpio_entry->class.item) {
		kfree(sw_gpio_entry);
		return -ENOMEM;
	}
	/* pr_info("gpio_name: %s\n", pdata->name); */
	gpio = of_get_named_gpio_flags(node, pdata->name, 0,
				       (enum of_gpio_flags *)sw_gpio_entry->
				       class.item);
	if (!gpio_is_valid(gpio)) {
		pr_err("get config err!\n");
		kfree(sw_gpio_entry->class.item);
		kfree(sw_gpio_entry);
		return -ENOMEM;
	}

	/* init the  */
	sw_gpio_entry->class.cfg  = (sw_gpio_entry->class.item)->mul_sel;
	sw_gpio_entry->class.pull = (sw_gpio_entry->class.item)->pull;
	sw_gpio_entry->class.drv  = (sw_gpio_entry->class.item)->drv_level;

	ret = map_gpio_to_name(io_area, sw_gpio_entry->class.item->gpio);
	pr_info("gpio name is %s, ret = %d\n", io_area, ret);

	platform_set_drvdata(dev, sw_gpio_entry);
	spin_lock_init(&sw_gpio_entry->lock);
	spin_lock_irqsave(&sw_gpio_entry->lock, flags);
	sw_gpio_entry->pdata = pdata;

	if (ret == 0)
		sw_gpio_entry->class.name = io_area;
	else
		sw_gpio_entry->class.name = pdata->name;

	sw_gpio_entry->class.gpio_sw_cfg_set = gpio_sw_cfg_set;
	sw_gpio_entry->class.gpio_sw_cfg_get = gpio_sw_cfg_get;
	sw_gpio_entry->class.gpio_sw_pull_set = gpio_sw_pull_set;
	sw_gpio_entry->class.gpio_sw_pull_get = gpio_sw_pull_get;
	sw_gpio_entry->class.gpio_sw_drv_set = gpio_sw_drv_set;
	sw_gpio_entry->class.gpio_sw_drv_get = gpio_sw_drv_get;
	sw_gpio_entry->class.gpio_sw_data_set = gpio_sw_data_set;
	sw_gpio_entry->class.gpio_sw_data_get = gpio_sw_data_get;


	/* init the gpio form sys_config */
	gpio_sw_cfg_set(&sw_gpio_entry->class,
			sw_gpio_entry->class.item->mul_sel);
	gpio_sw_data_set(&sw_gpio_entry->class,
			 sw_gpio_entry->class.item->data);

	spin_unlock_irqrestore(&sw_gpio_entry->lock, flags);

	ret = gpio_sw_classdev_register(&dev->dev, &sw_gpio_entry->class);
	if (ret < 0) {
		dev_err(&dev->dev, "gpio_sw_classdev_register failed\n");
		kfree(sw_gpio_entry->class.item);
		kfree(sw_gpio_entry);
		return -1;
	}

	/* create symbol link */
	if (strlen(pdata->link) != 0) {
		ret = sysfs_create_link(&gpio_sw_class->p->subsys.kobj,
					&sw_gpio_entry->class.dev->kobj,
					pdata->link);
	}
	return 0;
}

static struct platform_driver gpio_sw_driver = {
	.probe = gpio_sw_probe,
	.remove = gpio_sw_remove,
	.suspend = gpio_suspend,
	.resume = gpio_resume,
	.driver = {
		   .name = "gpio_sw",
		   .owner = THIS_MODULE,
		   },
};

static void gpio_sw_exit(void)
{
	int i, cnt;
	struct gpio_config config;
	int gpio;
	char gpio_name[32];
	int ret;

	platform_driver_unregister(&gpio_sw_driver);

	ret = of_property_read_u32(node, "gpio_num", &cnt);
	if (ret || !cnt) {
		pr_info("these is zero number for gpio\n");
		goto EXIT_END;
	}
	for (i = 0; i < cnt; i++) {
		sprintf(gpio_name, "gpio_pin_%d", i + 1);
		gpio =
		    of_get_named_gpio_flags(node, gpio_name, 0,
					    (enum of_gpio_flags *)&config);
		if (!gpio_is_valid(gpio)) {
			pr_err("this gpio is invalid: %d\n", gpio);
			continue;
		}

		platform_device_unregister(gpio_sw_dev[i]);
		kfree(gpio_sw_dev[i]);
		kfree(sw_pdata[i]);
		gpio_free(gpio);
	}

	class_destroy(gpio_sw_class);
EXIT_END:
	pr_info("gpio_exit finish !\n");
}

static int sunxi_init_gpio_probe(struct platform_device *pdev)
{
	int i, cnt;
	struct gpio_config config;
	int gpio;
	char gpio_name[32];
	int ret;
	const char *normal_led_pin_str = NULL;
	const char *standby_led_pin_str = NULL;
	const char *network_led_pin_str = NULL;

	node = pdev->dev.of_node;
	if (!node)
		goto INIT_END;

	/* create debug dir: /sys/class/gpio_sw */
	gpio_sw_class = class_create(THIS_MODULE, "gpio_sw");
	if (IS_ERR(gpio_sw_class))
		return PTR_ERR(gpio_sw_class);

	gpio_sw_class->suspend = gpio_sw_suspend;
	gpio_sw_class->resume = gpio_sw_resume;
	/*gpio_sw_class->class_attrs = gpio_sw_class_attrs;*/

	if (of_property_read_u32(node, "easy_light_used", &easy_light_used)) {
		easy_light_used = 0;
		pr_err("failed to get easy_light_used assign\n");
	}
	if (of_property_read_string(node, "normal_led", &normal_led_pin_str))
		pr_err("failed to get normal led pin assign\n");

	if (of_property_read_string(node, "standby_led", &standby_led_pin_str))
		pr_err("failed to get standby led pin assign\n");

	if (of_property_read_string(node, "network_led", &network_led_pin_str))
		pr_err("failed to get standby led pin assign\n");

	ret = of_property_read_u32(node, "gpio_num", &cnt);
	if (ret || !cnt) {
		pr_err("these is zero number for gpio\n");
		goto INIT_END;
	}

	for (i = 0; i < cnt; i++) {
		sprintf(gpio_name, "gpio_pin_%d", i + 1);
		gpio =
		    of_get_named_gpio_flags(node, gpio_name, 0,
					    (enum of_gpio_flags *)&config);
		/*printk(KERN_EMERG"gpio = %d, mul = %d, drv= %d, pull= %d, data = %d\n",\
				 config.gpio, config.mul_sel, config.drv_level, config.pull, config.data);*/
		if (gpio_request(gpio, NULL)) {
			pr_err("gpio_pin_%d(%d) gpio_request fail\n", i + 1,
			       gpio);
			continue;
		}
		pr_info("gpio_pin_%d(%d) gpio_is_valid\n", i + 1, config.gpio);

		sw_pdata[i] = kzalloc(sizeof(struct sw_gpio_pd), GFP_KERNEL);
		if (!sw_pdata[i]) {
			pr_err("kzalloc fail for sw_pdata[%d]\n", i);
			return -1;
		}

		gpio_sw_dev[i] =
		    kzalloc(sizeof(struct platform_device), GFP_KERNEL);
		if (!gpio_sw_dev[i]) {
			pr_err("kzalloc fail for gpio_sw_dev[%d]\n", i);
			return -1;
		}

		sprintf(sw_pdata[i]->name, "gpio_pin_%d", i + 1);
		if (normal_led_pin_str
		       && !strcmp(sw_pdata[i]->name, normal_led_pin_str)) {
			sprintf(sw_pdata[i]->link, "%s", "normal_led");
			if (easy_light_used)
				of_property_read_u32(node, "normal_led_light",  &sw_pdata[i]->light);
		} else if (standby_led_pin_str
			   && !strcmp(sw_pdata[i]->name, standby_led_pin_str)) {
			sprintf(sw_pdata[i]->link, "%s", "standby_led");
			if (easy_light_used)
				of_property_read_u32(node, "standby_led_light",  &sw_pdata[i]->light);
		} else if (network_led_pin_str
			   && !strcmp(sw_pdata[i]->name, network_led_pin_str)) {
			sprintf(sw_pdata[i]->link, "%s", "network_led");
			if (easy_light_used)
				of_property_read_u32(node, "network_led_light",  &sw_pdata[i]->light);
		}

		gpio_sw_dev[i]->name = "gpio_sw";
		gpio_sw_dev[i]->id = i;
		gpio_sw_dev[i]->dev.platform_data = sw_pdata[i];
		gpio_sw_dev[i]->dev.release = gpio_sw_release;

		if (platform_device_register(gpio_sw_dev[i])) {
			pr_err("%s platform_device_register fail\n",
			       sw_pdata[i]->name);
			goto INIT_ERR_FREE;
		}
	}
	if (platform_driver_register(&gpio_sw_driver)) {
		pr_err("gpio user platform_driver_register  fail\n");
		for (i = 0; i < cnt; i++)
			platform_device_unregister(gpio_sw_dev[i]);
		goto INIT_ERR_FREE;
	}

INIT_END:
	pr_info("gpio_init finish with uesd\n");
	return 0;
INIT_ERR_FREE:
	pr_err("gpio_init err\n");
	kfree(sw_pdata[i]);
	kfree(gpio_sw_dev[i]);
	return -1;
}

static const struct of_device_id sunxi_gpio_of_match[] = {
	{.compatible = "allwinner,sunxi-init-gpio", .data = NULL},
	{ /* sentinel */ }
};

static struct platform_driver sunxi_gpio_driver = {
	.driver = {
		   .name = "sunxi-init-gpio",
		   .of_match_table = of_match_ptr(sunxi_gpio_of_match),
		   },
	.probe = sunxi_init_gpio_probe,
};

static int __init sunxi_gpio_init(void)
{
	if (platform_driver_register(&sunxi_gpio_driver)) {
		pr_err("gpio user platform_driver_register  fail\n");
		return -1;
	}
	return 0;
}

static void __exit sunxi_gpio_exit(void)
{
	gpio_sw_exit();
	platform_driver_unregister(&sunxi_gpio_driver);
}

module_init(sunxi_gpio_init);
module_exit(sunxi_gpio_exit);

MODULE_AUTHOR("yanjianbo");
MODULE_DESCRIPTION("SW GPIO USER driver");
MODULE_LICENSE("GPL");
