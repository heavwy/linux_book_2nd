
#include <linux/module.h>
#include <linux/fs.h> /* struct file_operations */
/* platform_driver_register(), platform_set_drvdata() */
#include <linux/platform_device.h>
#include <linux/io.h> /* devm_ioremap(), writel_relaxed() */
#include <linux/of.h> /* of_property_read_string() */
#include <linux/uaccess.h> /* copy_from_user(), copy_to_user() */
#include <linux/miscdevice.h> /* misc_register() */
#include <linux/clk.h> /* clk_enable(), clock_disable() */

/* check the configuration of the pins */
/* root@stm32mp1:~# cat /sys/kernel/debug/pinctrl/soc\:pin-controller@50002000/pinconf-pins */

/* declare a private structure that will hold each led specific information */
struct led_device
{
	struct miscdevice led_misc_device; /* assign device for each led */
	u32 led_mask_set; /* different mask if led is R,G or B */
	u32 led_mask_clear;
	const char *led_name; /* assigned value cannot be modified */
	char led_value[8];
	struct leds_device *private; /* pointer to the global private struct */
	struct device *dev; 
};

/* declare a global private structure that will hold common information to all the led devices */
struct leds_device {
	u32 num_leds;
	struct clk *clk_gpioa;
	struct clk *clk_gpiog;
	struct led_device *leds[]; /* pointers to each led_device (R,G,B) private struct */
};

/* 
 * Allocate space for the global private struct 
 * including space for three pointers to led_device structures
 */
static inline int sizeof_led_priv(int num_leds)
{
	return sizeof(struct leds_device) +
			(sizeof(struct led_dev*) * num_leds);
}

/*
 * Green LED2: PA10, and Yellow LED3: PG8
 * gpioa: gpio@50002000 -> Green
 * gpiog: gpio@50008000 -> Yellow
 */

/* Declare physical PA addresses */
#define GPIOA_MODER  0x50002000  /* 0x00 offset */
#define GPIOA_OTYPER 0x50002004  /* 0x04 offset -> 0: Output push-pull */
#define GPIOA_PUPDR  0x5000200c  /* 0x0C offset -> 01:Pull-up, 10:Pull down */
#define GPIOA_BSRR   0x50002018  /* 0x18 offset */

/* Declare physical PD addresses */
#define GPIOG_MODER  0x50008000  /* 0x00 offset */
#define GPIOG_OTYPER 0x50008004  /* 0x04 offset -> 0: Output push-pull */
#define GPIOG_PUPDR  0x5000800c  /* 0x0C offset -> 01:Pull-up, 10:Pull down */
#define GPIOG_BSRR   0x50008018  /* 0x18 offset */

/* Declare __iomem pointers that will keep virtual addresses */
static void __iomem *GPIOA_MODER_V;
static void __iomem *GPIOA_OTYPER_V;
static void __iomem *GPIOA_PUPDR_V;
static void __iomem *GPIOA_BSRR_V;

static void __iomem *GPIOG_MODER_V;
static void __iomem *GPIOG_OTYPER_V;
static void __iomem *GPIOG_PUPDR_V;
static void __iomem *GPIOG_BSRR_V;

/* Green LED2: PA10 */
#define GPIOA_MODER_BSRR10_SET_Pos (10U)
#define GPIOA_MODER_BSRR10_CLEAR_Pos (26U)
#define GPIOA_MODER_MODER10_Pos (20U)
#define GPIOA_MODER_MODER10_0 (0x1U << GPIOA_MODER_MODER10_Pos)
#define GPIOA_MODER_MODER10_1 (0x2U << GPIOA_MODER_MODER10_Pos)
#define GPIOA_PUPDR_PUPDR10_0 (0x1U << GPIOA_MODER_MODER10_Pos)
#define GPIOA_PUPDR_PUPDR10_1 (0x2U << GPIOA_MODER_MODER10_Pos)

#define GPIOA_OTYPER_OTYPER10_pos (10U)
#define GPIOA_OTYPER_OTYPER10_Msk (0x1U << GPIOA_OTYPER_OTYPER10_pos)

#define GPIOA_PA10_SET_BSRR_Mask (1U << GPIOA_MODER_BSRR10_SET_Pos)
#define GPIOA_PA10_CLEAR_BSRR_Mask (1U << GPIOA_MODER_BSRR10_CLEAR_Pos)

/* Yellow LED3: PG8 */
#define GPIOG_MODER_BSRR8_SET_Pos (8U)
#define GPIOG_MODER_BSRR8_CLEAR_Pos (24U)
#define GPIOG_MODER_MODER8_Pos (16U)
#define GPIOG_MODER_MODER8_0 (0x1U << GPIOG_MODER_MODER8_Pos)
#define GPIOG_MODER_MODER8_1 (0x2U << GPIOG_MODER_MODER8_Pos)
#define GPIOG_PUPDR_PUPDR8_0 (0x1U << GPIOG_MODER_MODER8_Pos)
#define GPIOG_PUPDR_PUPDR8_1 (0x2U << GPIOG_MODER_MODER8_Pos)

#define GPIOG_OTYPER_OTYPER8_pos (8U)
#define GPIOG_OTYPER_OTYPER8_Msk (0x1U << GPIOG_OTYPER_OTYPER8_pos)

#define GPIOG_PG8_SET_BSRR_Mask (1U << GPIOG_MODER_BSRR8_SET_Pos)
#define GPIOG_PG8_CLEAR_BSRR_Mask (1U << GPIOG_MODER_BSRR8_CLEAR_Pos)


/* send on/off value from our terminal to control each led */
static ssize_t led_write(struct file *file, const char __user *buff,
						 size_t count, loff_t *ppos)
{
	const char *led_on = "on";
	const char *led_off = "off";
	struct led_device *led_device;
	struct leds_device *leds_device;

	pr_info("led_write() is called.\n");

	/* recover specific led_device structure (R,G,B) */
	led_device = container_of(file->private_data,
				  struct led_device, led_misc_device);
	
	/* recover the global private structure */
	leds_device = led_device->private;

	/*
	 * terminal echo add \n character.
	 * led_device->led_value = "on\n" or "off\n after copy_from_user"
	 * count = 3 for "on\n" and 4 for "off\n"
	 */
	if(copy_from_user(led_device->led_value, buff, count)) {
		pr_info("Bad copied value\n");
		return -EFAULT;
	}

	/*
	 * Replace \n for \0 in led_device->led_value
	 * char array to create a char string
	 */
	led_device->led_value[count-1] = '\0';

	pr_info("This message is received from User Space: %s\n",
			led_device->led_value);

	/* Enable the GPIO ports clocks */
	clk_enable(leds_device->clk_gpioa);
	clk_enable(leds_device->clk_gpiog);

	/* compare strings to switch on/off the LED, Green LED2: PA10, and Yellow LED3: PG8 */
	if(!strcmp(led_device->led_value, led_on)) {

		if(!strcmp(led_device->led_name, "ledgreen")) {
			writel_relaxed(led_device->led_mask_set, GPIOA_BSRR_V);
		}
		else if(!strcmp(led_device->led_name, "ledyellow")) {
			writel_relaxed(led_device->led_mask_set, GPIOG_BSRR_V);
		}
		else {
			pr_info("Bad value\n");
			return -EINVAL;
		}		
	}
	else if (!strcmp(led_device->led_value, led_off)) {
		
		if(!strcmp(led_device->led_name, "ledgreen")) {
			writel_relaxed(led_device->led_mask_clear, GPIOA_BSRR_V);
		}
		else if(!strcmp(led_device->led_name, "ledyellow")) {
			writel_relaxed(led_device->led_mask_clear, GPIOG_BSRR_V);
		}
		else {
			pr_info("Bad value\n");
			return -EINVAL;
		}		
	}
	else {
		pr_info("Bad value\n");
		return -EINVAL;
	}

	/* Disable the GPIO ports clocks */
	clk_disable(leds_device->clk_gpioa);
	clk_disable(leds_device->clk_gpiog);

	pr_info("led_write() is exit.\n");
	return count;
}

/*
 * read each LED status on/off
 * use cat from terminal to read
 * led_read is entered until *ppos > 0
 * twice in this function
 */
static ssize_t led_read(struct file *file, char __user *buff,
						size_t count, loff_t *ppos)
{
	int len;
	struct led_device *led_device;

	led_device = container_of(file->private_data, struct led_device,
							  led_misc_device);

	if(*ppos == 0){
		len = strlen(led_device->led_value);
		pr_info("the size of the message is %d\n", len); /* 2 for on */
		led_device->led_value[len] = '\n'; /* add \n after on/off */
		if(copy_to_user(buff, &led_device->led_value, len+1)){
			pr_info("Failed to return led_value to user space\n");
			return -EFAULT;
		}
		*ppos+=1; /* increment *ppos to exit the function in next call */
		return sizeof(led_device->led_value); /* exit first func call */
	}

	return 0; /* exit and do not recall func again */
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.read = led_read,
	.write = led_write,
};

static int led_probe(struct platform_device *pdev)
{

	u32 GPIOA_MODER_write, GPIOG_MODER_write;
	u32 GPIOA_OTYPER_write, GPIOG_OTYPER_write;
	u32 GPIOA_PUPDR_write, GPIOG_PUPDR_write;
	int ret_val, count;
	struct device_node *child;
	
	/* declare global private structure */
	struct leds_device *leds_device;

	struct device *dev = &pdev->dev;

	/* initialize all the leds to off */
	char led_val[8] = "off\n";

	pr_info("led_probe enter\n");

	/* get the number of led devices declared in the device tree */
	count = device_get_child_node_count(dev);
	if (!count)
		return -ENODEV;

	dev_info(dev, "there are %d nodes\n", count);

	/* Allocate space for the global private structure + pointers to led_device structures */
	leds_device = devm_kzalloc(dev, sizeof_led_priv(count), GFP_KERNEL);
	if (!leds_device)
		return -ENOMEM;

	/* Get the clocks from the device tree and store them in the global structure */
	leds_device->clk_gpioa = devm_clk_get(&pdev->dev, "GPIOA");
	if (IS_ERR(leds_device->clk_gpioa)) {
		dev_err(&pdev->dev, "failed to get clk gpioa (%ld)\n", PTR_ERR(leds_device->clk_gpioa));
		return PTR_ERR(leds_device->clk_gpioa);
	}

	leds_device->clk_gpiog = devm_clk_get(&pdev->dev, "GPIOG");
	if (IS_ERR(leds_device->clk_gpiog)) {
		dev_err(&pdev->dev, "failed to get clk gpiog (%ld)\n", PTR_ERR(leds_device->clk_gpiog));
		return PTR_ERR(leds_device->clk_gpiog);
	}

	ret_val = clk_prepare(leds_device->clk_gpioa);
	if (ret_val) {
		dev_err(&pdev->dev, "failed to prepare clk (%d)\n", ret_val);
		return ret_val;
	}

	ret_val = clk_prepare(leds_device->clk_gpiog);
	if (ret_val) {
		dev_err(&pdev->dev, "failed to prepare clk (%d)\n", ret_val);
		return ret_val;
	}

	/* Enable the clocks to configure the GPIO registers */
	clk_enable(leds_device->clk_gpioa);
	clk_enable(leds_device->clk_gpiog);
	
	/* Get virtual addresses */
	GPIOA_MODER_V = devm_ioremap(&pdev->dev, GPIOA_MODER, sizeof(u32));
	GPIOA_OTYPER_V = devm_ioremap(&pdev->dev, GPIOA_OTYPER, sizeof(u32));
	GPIOA_PUPDR_V = devm_ioremap(&pdev->dev, GPIOA_PUPDR, sizeof(u32));
	GPIOA_BSRR_V = devm_ioremap(&pdev->dev, GPIOA_BSRR, sizeof(u32));

	GPIOG_MODER_V = devm_ioremap(&pdev->dev, GPIOG_MODER, sizeof(u32));
	GPIOG_OTYPER_V = devm_ioremap(&pdev->dev, GPIOG_OTYPER, sizeof(u32));
	GPIOG_PUPDR_V = devm_ioremap(&pdev->dev, GPIOG_PUPDR, sizeof(u32));
	GPIOG_BSRR_V = devm_ioremap(&pdev->dev, GPIOG_BSRR, sizeof(u32));

	/* ensures that all leds are off when GPIOs are configured to GP output */
	writel_relaxed(GPIOA_PA10_SET_BSRR_Mask, GPIOA_BSRR_V);
	writel_relaxed(GPIOG_PG8_SET_BSRR_Mask, GPIOG_BSRR_V);

	/* set PA13 to GP output */
	GPIOA_MODER_write = readl_relaxed(GPIOA_MODER_V);
	GPIOA_MODER_write |= GPIOA_MODER_MODER10_0; 
	GPIOA_MODER_write &= ~(GPIOA_MODER_MODER10_1);
	
	writel_relaxed(GPIOA_MODER_write, GPIOA_MODER_V);

	/* set PG8 to GP output */
	GPIOG_MODER_write = readl_relaxed(GPIOG_MODER_V);
	GPIOG_MODER_write |= GPIOG_MODER_MODER8_0; 
	GPIOG_MODER_write &= ~(GPIOG_MODER_MODER8_1);
	
	writel_relaxed(GPIOG_MODER_write, GPIOG_MODER_V);

	/* set PA10 to PP (push-pull) configuration */
	GPIOA_OTYPER_write = readl_relaxed(GPIOA_OTYPER_V);
	GPIOA_OTYPER_write &= ~(GPIOA_OTYPER_OTYPER10_Msk);

	writel_relaxed(GPIOA_OTYPER_write, GPIOA_OTYPER_V);

	/* set PG8 to PP (push-pull) configuration */
	GPIOG_OTYPER_write = readl_relaxed(GPIOG_OTYPER_V);
	GPIOG_OTYPER_write &= ~(GPIOG_OTYPER_OTYPER8_Msk);

	writel_relaxed(GPIOG_OTYPER_write, GPIOG_OTYPER_V);

	/* set PA10 PU */
	GPIOA_PUPDR_write = readl_relaxed(GPIOA_PUPDR_V);
	GPIOA_PUPDR_write |= GPIOA_PUPDR_PUPDR10_0; 
	GPIOA_PUPDR_write &= ~(GPIOA_PUPDR_PUPDR10_1);

	writel_relaxed(GPIOA_PUPDR_write, GPIOA_PUPDR_V);
	
	/* set PG8 PU */
	GPIOG_PUPDR_write = readl_relaxed(GPIOG_PUPDR_V);
	GPIOG_PUPDR_write |= GPIOG_PUPDR_PUPDR8_0; 
	GPIOG_PUPDR_write &= ~(GPIOG_PUPDR_PUPDR8_1);	

	writel_relaxed(GPIOG_PUPDR_write, GPIOG_PUPDR_V);

	/* Disable the clocks after configuring the registers */
	clk_disable(leds_device->clk_gpioa);
	clk_disable(leds_device->clk_gpiog);

	/* Get the information of each led node from the device tree */
	for_each_child_of_node(dev->of_node, child){
		/* declare a led structure for each led device */
		struct led_device *led_device;

		/* Allocate space for each led device structure */
		led_device = devm_kzalloc(dev, sizeof(*led_device), GFP_KERNEL);
		if (!led_device)
			return -ENOMEM;

		/* Allocate space for each led device structure */
		of_property_read_string(child, "label", &led_device->led_name);

		/* Create a misc device for each led device structure and store the specific set/clear masks */
		if (strcmp(led_device->led_name,"ledgreen") == 0) {
			led_device->led_misc_device.minor = MISC_DYNAMIC_MINOR;
			led_device->led_misc_device.name = led_device->led_name;
			led_device->led_misc_device.fops = &led_fops;
			led_device->led_mask_set = GPIOA_PA10_CLEAR_BSRR_Mask;
			led_device->led_mask_clear = GPIOA_PA10_SET_BSRR_Mask;
		}
		else if (strcmp(led_device->led_name,"ledyellow") == 0) {
			led_device->led_misc_device.minor = MISC_DYNAMIC_MINOR;
			led_device->led_misc_device.name = led_device->led_name;
			led_device->led_misc_device.fops = &led_fops;
			led_device->led_mask_set = GPIOG_PG8_CLEAR_BSRR_Mask;
			led_device->led_mask_clear = GPIOG_PG8_SET_BSRR_Mask;
		}
		else {
			dev_info(dev, "Bad device tree value\n");
			return -EINVAL;
		}

		/* Initialize each led status to off */
		memcpy(led_device->led_value, led_val, sizeof(led_val));

		/* register each led device */
		ret_val = misc_register(&led_device->led_misc_device);
		if (ret_val) return ret_val; /* misc_register returns 0 if success */
		pr_info("the led number %d is registered\n", leds_device->num_leds);

		/* 
                 * each led_device will store a pointer to the global leds_device struct
                 * to recover this struct in each write() function 
                 */
		led_device->private = leds_device; 

		/* 
		 * In the leds_device "leds" pointers points to each new led_device struct
                 * to be able to unregister misc devices in the remove() function 
                 */
		leds_device->leds[leds_device->num_leds] = led_device; 
		leds_device->num_leds++;

	}

	/* to recover leds_device global structure in remove() */
	platform_set_drvdata(pdev, leds_device); 

	pr_info("leds_probe exit\n");

	return 0;
}

static int led_remove(struct platform_device *pdev)
{
	int i;
	struct led_device *led_count;

	/* recover leds_device global structure */
	struct leds_device *leds_device = platform_get_drvdata(pdev);

	pr_info("leds_remove enter\n");

	/* Unregister each led device (R,G,B) */
	for (i = 0; i < leds_device->num_leds; i++) {
		led_count = leds_device->leds[i];
		misc_deregister(&led_count->led_misc_device);
		pr_info("the led number %d is unregistered\n", i);
	}

	/* Enable the clocks to configure GPIO registers */
	clk_enable(leds_device->clk_gpioa);
	clk_enable(leds_device->clk_gpiog);

	/* ensures that all leds are off when changes the GPIO to GP output */
	writel_relaxed(GPIOA_PA10_SET_BSRR_Mask, GPIOA_BSRR_V);
	writel_relaxed(GPIOG_PG8_SET_BSRR_Mask, GPIOG_BSRR_V);

	/* Disable the clocks */
	clk_disable(leds_device->clk_gpioa);
	clk_disable(leds_device->clk_gpiog);

	pr_info("leds_remove exit\n");

	return 0;
}

static const struct of_device_id my_of_ids[] = {
	{ .compatible = "arrow,RGBleds"},
	{},
};
MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver led_platform_driver = {
	.probe = led_probe,
	.remove = led_remove,
	.driver = {
		.name = "RGBleds",
		.of_match_table = my_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(led_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alberto Liberal <aliberal@arroweurope.com>");
MODULE_DESCRIPTION("This is a platform driver that turns on/off \
					three led devices");

