#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/signal.h>
#include <linux/pm.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#define TOPBAND_GPIO_NAME "topband,gpio"
#define TOPBAND_GPIO_MAX 20
#define TOPBAND_GPIO_DIRECTION_MASK 0x01
#define TOPBAND_GPIO_VALUE_MASK 0x02

// ioctl cmd
#define TOPBAND_GPIO_IOC_MAGIC  'f'

#define TOPBAND_GPIO_IOC_SET_VALUE _IOW(TOPBAND_GPIO_IOC_MAGIC, 1, int)
#define TOPBAND_GPIO_IOC_GET_VALUE _IOR(TOPBAND_GPIO_IOC_MAGIC, 2, int)
#define TOPBAND_GPIO_IOC_SET_DIRECTION _IOW(TOPBAND_GPIO_IOC_MAGIC, 3, int)
#define TOPBAND_GPIO_IOC_REG_KEY_EVENT _IOW(TOPBAND_GPIO_IOC_MAGIC, 4, int)
#define TOPBAND_GPIO_IOC_UNREG_KEY_EVENT _IOW(TOPBAND_GPIO_IOC_MAGIC, 5, int)
#define TOPBAND_GPIO_IOC_GET_NUMBER _IOW(TOPBAND_GPIO_IOC_MAGIC, 6, int)

#define TOPBAND_GPIO_IOC_MAXNR 6

#define TOPBAND_GPIO_CONFIG_OUTPUT_LOW 0
#define TOPBAND_GPIO_CONFIG_OUTPUT_HIGHT 1
#define TOPBAND_GPIO_CONFIG_INPUT 2

static int gKeyCode[TOPBAND_GPIO_MAX] = {
    KEY_GPIO_0,
    KEY_GPIO_1, 
    KEY_GPIO_2, 
    KEY_GPIO_3, 
    KEY_GPIO_4, 
    KEY_GPIO_5, 
    KEY_GPIO_6, 
    KEY_GPIO_7, 
    KEY_GPIO_8, 
    KEY_GPIO_9};

struct topband_gpio {
    int gpio;
    int config;
};

struct topband_gpio_data {
    struct platform_device *platform_dev;
    struct miscdevice topband_gpio_device;
    struct input_dev *input_dev;
    struct topband_gpio gpios[TOPBAND_GPIO_MAX];
    int irqs[TOPBAND_GPIO_MAX];
    int gpio_number;
};

static void topband_gpio_free_io_irq(struct topband_gpio_data *topband_gpio, int gpio)
{
    if (gpio >= 0 && gpio < topband_gpio->gpio_number) {
        if (topband_gpio->irqs[gpio] > 0) {
            free_irq(topband_gpio->irqs[gpio], topband_gpio);
            topband_gpio->irqs[gpio] = -1;
        }
    }
}

static void topband_gpio_free_irq(struct topband_gpio_data *topband_gpio)
{
    int i;
    for (i=0; i<topband_gpio->gpio_number; i++) {
        topband_gpio_free_io_irq(topband_gpio, i);
    }
}

static void topband_gpio_free_io_port(struct topband_gpio_data *topband_gpio)
{
    int i;
    for (i=0; i<topband_gpio->gpio_number; i++) {
        if(gpio_is_valid(topband_gpio->gpios[i].gpio)) {
            gpio_free(topband_gpio->gpios[i].gpio);
        }
    }
    return;
}

static int topband_gpio_parse_dt(struct device *dev,
                              struct topband_gpio_data *topband_gpio)
{
    int ret = 0, index = 0;
    struct device_node *np = dev->of_node;
    struct device_node *root  = of_get_child_by_name(np, "topband,gpios");
    struct device_node *child;

    for_each_child_of_node(root, child) {
        if (index >= TOPBAND_GPIO_MAX) {
            dev_err(dev, "The number of GPIOs exceeds the maximum:%d value to break", TOPBAND_GPIO_MAX);
            break;
        }
        
        topband_gpio->gpios[index].gpio = of_get_named_gpio(child, "topband,gpio", 0);
        if(!gpio_is_valid(topband_gpio->gpios[index].gpio)) {
            dev_err(dev, "No valid gpio[%d]", index);
            return -1;
        }

        ret = of_property_read_u32(child, "topband,config", &topband_gpio->gpios[index].config);
        if(ret < 0 || topband_gpio->gpios[index].config < TOPBAND_GPIO_CONFIG_OUTPUT_LOW 
            || topband_gpio->gpios[index].config > TOPBAND_GPIO_CONFIG_INPUT) {
            dev_err(dev, "No valid gpio[%d]'s config", index);
            return -1;
        }

        index++;
	}

    topband_gpio->gpio_number = index;

    return 0;
}

static int topband_gpio_request_io_port(struct topband_gpio_data *topband_gpio)
{
    int ret = 0;
    int i;
    for (i=0; i<topband_gpio->gpio_number; i++) {
        if(gpio_is_valid(topband_gpio->gpios[i].gpio)) {
            ret = gpio_request(topband_gpio->gpios[i].gpio, "topband_gpio");
            if(ret < 0) {
                dev_err(&topband_gpio->platform_dev->dev,
                        "Failed to request GPIO[%d]:%d, ERRNO:%d\n",
                        i, (s32)topband_gpio->gpios[i].gpio, ret);
                return -ENODEV;
            }

            if (topband_gpio->gpios[i].config == TOPBAND_GPIO_CONFIG_INPUT) {
                gpio_direction_input(topband_gpio->gpios[i].gpio);
            } else if (topband_gpio->gpios[i].config == TOPBAND_GPIO_CONFIG_OUTPUT_LOW) {
                gpio_direction_output(topband_gpio->gpios[i].gpio, 0);
            } else if (topband_gpio->gpios[i].config == TOPBAND_GPIO_CONFIG_OUTPUT_HIGHT) {
                gpio_direction_output(topband_gpio->gpios[i].gpio, 1);
            } 
            
            dev_info(&topband_gpio->platform_dev->dev, "Success request gpio[%d]\n", i);
        }
    }

    return ret;
}

static s8 topband_gpio_request_input_dev(struct topband_gpio_data *topband_gpio)
{
    s8 ret = -1;

    topband_gpio->input_dev = input_allocate_device();

    if(topband_gpio->input_dev == NULL) {
        dev_err(&topband_gpio->platform_dev->dev, "Failed to allocate input device\n");
        return -ENOMEM;
    }

    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_0);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_1);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_2);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_3);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_4);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_5);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_6);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_7);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_8);
    input_set_capability(topband_gpio->input_dev, EV_KEY, KEY_GPIO_9);

    ret = input_register_device(topband_gpio->input_dev);

    if(ret) {
        dev_err(&topband_gpio->platform_dev->dev, "Register %s input device failed\n",
                topband_gpio->input_dev->name);
        input_free_device(topband_gpio->input_dev);
        return -ENODEV;
    }

    return 0;
}


static int topband_gpio_set_value(int gpio, int value) {
    if(gpio_is_valid(gpio)) {
        gpio_set_value(gpio, value);
        return 0;
    }
    return -1;
}

static int topband_gpio_get_value(int gpio) {
    if(gpio_is_valid(gpio)) {
        return gpio_get_value(gpio);
    }
    return -1;
}

static int topband_gpio_set_direction(int gpio, int value) {
    int direction = 0;
    int data = 0;
    
    if(gpio_is_valid(gpio)) {
        direction = value & TOPBAND_GPIO_DIRECTION_MASK;
        data = value & TOPBAND_GPIO_VALUE_MASK;
        if (direction > 0) {
            return gpio_direction_output(gpio, data);
        } else {
            return gpio_direction_input(gpio);
        }
    }
    return -1;
}


static int topband_gpio_write(struct topband_gpio_data *topband_gpio, int gpio, int value) {
    int ret = -1;

    if (gpio < topband_gpio->gpio_number) {
       ret = topband_gpio_set_value(topband_gpio->gpios[gpio].gpio, value); 
    }

    return ret;
}

static int topband_gpio_read(struct topband_gpio_data *topband_gpio, int gpio) {
    int ret = -1;

    if (gpio < topband_gpio->gpio_number) {
       ret = topband_gpio_get_value(topband_gpio->gpios[gpio].gpio); 
    }

    return ret;
}

static int topband_gpio_direction(struct topband_gpio_data *topband_gpio, int gpio, int value) {
    int ret = -1;

    if (gpio < topband_gpio->gpio_number) {
       ret = topband_gpio_set_direction(topband_gpio->gpios[gpio].gpio, value); 
    }

    return ret;
}

static irqreturn_t topband_gpio_irq_handle(int irq, void *dev_id)
{
    struct topband_gpio_data *topband_gpio = dev_id;
    int gpio = -1;
    int value = -1;
    int i;

    for (i=0; i<topband_gpio->gpio_number; i++) {
        if (irq == topband_gpio->irqs[i]) {
            gpio = i;
        }
    }

    if (gpio >= 0 && gpio < topband_gpio->gpio_number) {
        value = topband_gpio_get_value(topband_gpio->gpios[gpio].gpio);
        if (value >= 0) {
           input_report_key(topband_gpio->input_dev, gKeyCode[gpio], !value);
           input_sync(topband_gpio->input_dev); 
        }
    }

    return IRQ_HANDLED;
}

static int topband_gpio_request_irq(struct topband_gpio_data *topband_gpio, int gpio)
{
    int ret = 0;

    /* use irq */
    if(gpio_is_valid(topband_gpio->gpios[gpio].gpio) || topband_gpio->irqs[gpio] > 0) {
        if(gpio_is_valid(topband_gpio->gpios[gpio].gpio))
            topband_gpio->irqs[gpio] = gpio_to_irq(topband_gpio->gpios[gpio].gpio);

        dev_info(&topband_gpio->platform_dev->dev, "INT num %d, trigger type:%d\n",
                 topband_gpio->irqs[gpio], IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
        ret = request_threaded_irq(topband_gpio->irqs[gpio], NULL,
                                   topband_gpio_irq_handle,
                                   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                   topband_gpio->platform_dev->name,
                                   topband_gpio);

        if(ret < 0) {
            dev_err(&topband_gpio->platform_dev->dev,
                    "Failed to request irq %d\n", topband_gpio->irqs[gpio]);
        }
    }

    return ret;
}

static int topband_gpio_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct topband_gpio_data *topband_gpio = container_of(filp->private_data,
							   struct topband_gpio_data,
							   topband_gpio_device);
	filp->private_data = topband_gpio;
	dev_info(&topband_gpio->platform_dev->dev,
		 "device node major=%d, minor=%d\n", imajor(inode), iminor(inode));

	return ret;
}

static long topband_gpio_dev_ioctl(struct file *pfile,
					 unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int data = 0;
    int gpio = 0, value = 0;
	struct topband_gpio_data *topband_gpio = pfile->private_data;

    if (_IOC_TYPE(cmd) != TOPBAND_GPIO_IOC_MAGIC) 
        return -EINVAL;
    if (_IOC_NR(cmd) > TOPBAND_GPIO_IOC_MAXNR) 
        return -EINVAL;

    if (_IOC_DIR(cmd) & _IOC_READ)
        ret = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        ret = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if (ret) 
        return -EFAULT;

    if (copy_from_user(&data, (int *)arg, sizeof(int))) {
        dev_err(&topband_gpio->platform_dev->dev, 
            "%s, copy from user failed\n", __func__);
        return -EFAULT;
    }
    gpio = (data >> 4) & 0x0f;
    value = data & 0x0f;
    
    dev_info(&topband_gpio->platform_dev->dev,
                 "%s, (%x, %lx): gpio=%d, value=%d\n", __func__, cmd,
                 arg, gpio, value);
    
	switch (cmd) {
    	case TOPBAND_GPIO_IOC_SET_VALUE:
            ret = topband_gpio_write(topband_gpio, gpio, value);
    		break;

        case TOPBAND_GPIO_IOC_GET_VALUE:
            ret = topband_gpio_read(topband_gpio, gpio);
            if (ret >= 0) {
                if (copy_to_user((int *)arg, &ret, sizeof(int))) {
                    dev_err(&topband_gpio->platform_dev->dev, 
                        "%s, copy to user failed\n", __func__);
                    return -EFAULT;
                }
            } else {
                dev_err(&topband_gpio->platform_dev->dev, 
                        "%s, gpio get value failed\n", __func__);
                    return -EFAULT;
            }
            break;

        case TOPBAND_GPIO_IOC_SET_DIRECTION:
            ret = topband_gpio_direction(topband_gpio, gpio, value);
            break;

        case TOPBAND_GPIO_IOC_REG_KEY_EVENT:
            ret = topband_gpio_direction(topband_gpio, gpio, 0); // set input
            if (ret >= 0) {
                ret = topband_gpio_request_irq(topband_gpio, gpio);
            } else {
               dev_err(&topband_gpio->platform_dev->dev, 
                        "%s, reg key event set gpio input failed\n", __func__); 
            }
            break;

        case TOPBAND_GPIO_IOC_UNREG_KEY_EVENT:
            topband_gpio_free_io_irq(topband_gpio, gpio);
            break;

        case TOPBAND_GPIO_IOC_GET_NUMBER:
            if (copy_to_user((int *)arg, &topband_gpio->gpio_number, sizeof(int))) {
                dev_err(&topband_gpio->platform_dev->dev, 
                    "%s, copy to user failed\n", __func__);
                return -EFAULT;
            }
            break;
 
    	default:
    		return -EINVAL;
	}

	return ret;
}

static const struct file_operations topband_gpio_dev_fops = {
	.owner = THIS_MODULE,
    .open = topband_gpio_dev_open,
	.unlocked_ioctl = topband_gpio_dev_ioctl
};

static int topband_gpio_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct topband_gpio_data *topband_gpio;

    topband_gpio = devm_kzalloc(&pdev->dev, sizeof(*topband_gpio), GFP_KERNEL);

    if(topband_gpio == NULL) {
        dev_err(&pdev->dev, "Failed alloc ts memory");
        return -ENOMEM;
    }

    if(pdev->dev.of_node) {
        ret = topband_gpio_parse_dt(&pdev->dev, topband_gpio);

        if(ret) {
            dev_err(&pdev->dev, "Failed parse dts\n");
            goto exit_free_data;
        }
    }

    topband_gpio->platform_dev = pdev;

    ret = topband_gpio_request_io_port(topband_gpio);

    if(ret < 0) {
        dev_err(&pdev->dev, "Failed request IO port\n");
        goto exit_free_data;
    }

    ret = topband_gpio_request_input_dev(topband_gpio);

    if(ret < 0) {
        dev_err(&pdev->dev, "Failed request IO port\n");
        goto exit_free_io_port;
    }

    platform_set_drvdata(pdev, topband_gpio);

    topband_gpio->topband_gpio_device.minor = MISC_DYNAMIC_MINOR;
	topband_gpio->topband_gpio_device.name = "topband_gpio";
	topband_gpio->topband_gpio_device.fops = &topband_gpio_dev_fops;

	ret = misc_register(&topband_gpio->topband_gpio_device);
	if (ret) {
		dev_err(&pdev->dev, "Failed misc_register\n");
		goto exit_unreg_input_dev;
	}

    dev_info(&pdev->dev, "%s, over\n", __func__);
    return 0;
    
exit_unreg_input_dev:
    input_unregister_device(topband_gpio->input_dev);

exit_free_io_port:
    topband_gpio_free_io_port(topband_gpio);
    
exit_free_data:
    devm_kfree(&pdev->dev, topband_gpio);

    return ret;
}

static int topband_gpio_remove(struct platform_device *pdev)
{
    struct topband_gpio_data *topband_gpio = platform_get_drvdata(pdev);
    topband_gpio_free_irq(topband_gpio);
    topband_gpio_free_io_port(topband_gpio);
    kfree(topband_gpio);

    return 0;
}

static const struct of_device_id topband_gpio_of_match[] = {
    { .compatible =  "topband,gpio"},
    {},
};

MODULE_DEVICE_TABLE(of, topband_gpio_of_match);

static struct platform_driver topband_gpio_driver = {
    .probe = topband_gpio_probe,
    .remove = topband_gpio_remove,
    .driver = {
        .name = TOPBAND_GPIO_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = topband_gpio_of_match,
    },
};

module_platform_driver(topband_gpio_driver);

MODULE_AUTHOR("shenhb@topband.com");
MODULE_DESCRIPTION("Topband GPIO for application control");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
