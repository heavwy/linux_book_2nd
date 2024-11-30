
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/leds.h>
#include <linux/clk.h> 

/* Declare physical PA addresses offsets */
#define GPIOA_MODER_offset 0x00 /* 0x00 offset */
#define GPIOA_OTYPER_offset 0x04 /* 0x04 offset -> 0: Output push-pull */
#define GPIOA_PUPDR_offset 0x0c  /* 0x0C offset -> 01:Pull-up, 10:Pull down */
#define GPIOA_BSRR_offset 0x18 /* 0x18 offset */

/* Declare physical PD addresses offsets */
#define GPIOG_MODER_offset 0x00 /* 0x00 offset */
#define GPIOG_OTYPER_offset 0x04 /* 0x04 offset -> 0: Output push-pull */
#define GPIOG_PUPDR_offset 0x0c  /* 0x0C offset -> 01:Pull-up, 10:Pull down */
#define GPIOG_BSRR_offset 0x18 /* 0x18 offset */

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

struct led_device
{
	u32 led_mask_set; 
	u32 led_mask_clear;
	const char *led_name;
	void __iomem *base_gpioa;
	void __iomem *base_gpiog;
	struct clk *clk_gpioa;
	struct clk *clk_gpiog;
	struct led_classdev cdev;
};

static void led_control(struct led_classdev *led_cdev, enum led_brightness b)
{

	struct led_device *led = container_of(led_cdev, struct led_device, cdev);

	/* Enable the clocks to configure the GPIO registers */
	clk_enable(led->clk_gpioa);
	clk_enable(led->clk_gpiog);

	if (b != LED_OFF) {	/* LED ON */
		if(!strcmp(led->led_name, "ledgreen"))
			writel_relaxed(led->led_mask_set, led->base_gpioa + GPIOA_BSRR_offset);
		else if(!strcmp(led->led_name, "ledyellow"))
			writel_relaxed(led->led_mask_set, led->base_gpiog + GPIOG_BSRR_offset);
		else 
			pr_info("Bad value\n");
	}
	else {
		if(!strcmp(led->led_name, "ledgreen"))
			writel_relaxed(led->led_mask_clear, led->base_gpioa + GPIOA_BSRR_offset);
		else if(!strcmp(led->led_name, "ledyellow"))
			writel_relaxed(led->led_mask_clear, led->base_gpiog + GPIOG_BSRR_offset);
		else 
			pr_info("Bad value\n");
	}

	/* Disable the clocks to configure the GPIO registers */
	clk_disable(led->clk_gpioa);
	clk_disable(led->clk_gpiog);
}

static int ledclass_probe(struct platform_device *pdev)
{

	void __iomem *g_ioremap_addr_gpioa;
	void __iomem *g_ioremap_addr_gpiog;
	struct device_node *child;
	struct resource *r_gpioa;
	struct resource *r_gpiog;
	struct clk *clk_gpioa;
	struct clk *clk_gpiog;
	u32 GPIOA_MODER_write, GPIOG_MODER_write;
	u32 GPIOA_OTYPER_write, GPIOG_OTYPER_write;
	u32 GPIOA_PUPDR_write, GPIOG_PUPDR_write;
	int ret_val, count;
	
	struct device *dev = &pdev->dev;

	dev_info(dev, "platform_probe enter\n");


	/* Get the clocks from the device tree and store them in the global structure */
	clk_gpioa = devm_clk_get(&pdev->dev, "GPIOA");
	if (IS_ERR(clk_gpioa)) {
		dev_err(&pdev->dev, "failed to get clk (%ld)\n", PTR_ERR(clk_gpioa));
		return PTR_ERR(clk_gpioa);
	}

	clk_gpiog = devm_clk_get(&pdev->dev, "GPIOG");
	if (IS_ERR(clk_gpiog)) {
		dev_err(&pdev->dev, "failed to get clk (%ld)\n", PTR_ERR(clk_gpiog));
		return PTR_ERR(clk_gpiog);
	}

	ret_val = clk_prepare(clk_gpioa);
	if (ret_val) {
		dev_err(&pdev->dev, "failed to prepare clk (%d)\n", ret_val);
		return ret_val;
	}

	ret_val = clk_prepare(clk_gpiog);
	if (ret_val) {
		dev_err(&pdev->dev, "failed to prepare clk (%d)\n", ret_val);
		return ret_val;
	}

	/* get our first memory resource from device tree */
	r_gpioa = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_gpioa) {
		dev_err(dev, "IORESOURCE_MEM, 0 does not exist\n");
		return -EINVAL;
	}
	dev_info(dev, "r->start = 0x%08lx\n", (long unsigned int)r_gpioa->start);
	dev_info(dev, "r->end = 0x%08lx\n", (long unsigned int)r_gpioa->end);

	/* get our second memory resource from device tree */
	r_gpiog = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!r_gpiog) {
		dev_err(dev, "IORESOURCE_MEM, 0 does not exist\n");
		return -EINVAL;
	}
	dev_info(dev, "r->start = 0x%08lx\n", (long unsigned int)r_gpiog->start);
	dev_info(dev, "r->end = 0x%08lx\n", (long unsigned int)r_gpiog->end);

	/* ioremap our memory region */
	g_ioremap_addr_gpioa = devm_ioremap(dev, r_gpioa->start, resource_size(r_gpioa));
	if (!g_ioremap_addr_gpioa) {
		dev_err(dev, "ioremap failed \n");
		return -ENOMEM;
	}

	/* ioremap our memory region */
	g_ioremap_addr_gpiog = devm_ioremap(dev, r_gpiog->start, resource_size(r_gpiog));
	if (!g_ioremap_addr_gpiog) {
		dev_err(dev, "ioremap failed \n");
		return -ENOMEM;
	}

	/* Enable the clocks to configure the GPIO registers */
	clk_enable(clk_gpioa);
	clk_enable(clk_gpiog);

	/* ensures that all leds are off when GPIOs are configured to GP output */
	writel_relaxed(GPIOA_PA10_SET_BSRR_Mask, (g_ioremap_addr_gpioa + GPIOA_BSRR_offset));
	writel_relaxed(GPIOG_PG8_SET_BSRR_Mask, (g_ioremap_addr_gpiog + GPIOG_BSRR_offset));

	/* set PA10 to GP output */
	GPIOA_MODER_write = readl_relaxed(g_ioremap_addr_gpioa + GPIOA_MODER_offset);
	GPIOA_MODER_write |= GPIOA_MODER_MODER10_0; 
	GPIOA_MODER_write &= ~(GPIOA_MODER_MODER10_1);
	
	writel_relaxed(GPIOA_MODER_write, (g_ioremap_addr_gpioa + GPIOA_MODER_offset));

	/* set PG8 to GP output */
	GPIOG_MODER_write = readl_relaxed(g_ioremap_addr_gpiog + GPIOG_MODER_offset);
	GPIOG_MODER_write |= GPIOG_MODER_MODER8_0; 
	GPIOG_MODER_write &= ~(GPIOG_MODER_MODER8_1);
	
	writel_relaxed(GPIOG_MODER_write, (g_ioremap_addr_gpiog + GPIOG_MODER_offset));

	/* set PA10 to PP (push-pull) configuration */
	GPIOA_OTYPER_write = readl_relaxed(g_ioremap_addr_gpioa + GPIOA_OTYPER_offset);
	GPIOA_OTYPER_write &= ~(GPIOA_OTYPER_OTYPER10_Msk);

	writel_relaxed(GPIOA_OTYPER_write, (g_ioremap_addr_gpioa + GPIOA_OTYPER_offset));

	/* set PG8 to PP (push-pull) configuration */
	GPIOG_OTYPER_write = readl_relaxed(g_ioremap_addr_gpiog + GPIOG_OTYPER_offset);
	GPIOG_OTYPER_write &= ~(GPIOG_OTYPER_OTYPER8_Msk);

	writel_relaxed(GPIOG_OTYPER_write, (g_ioremap_addr_gpiog + GPIOG_OTYPER_offset));

	/* set PA10 PU */
	GPIOA_PUPDR_write = readl_relaxed(g_ioremap_addr_gpioa + GPIOA_PUPDR_offset);
	GPIOA_PUPDR_write |= GPIOA_PUPDR_PUPDR10_0; 
	GPIOA_PUPDR_write &= ~(GPIOA_PUPDR_PUPDR10_1);

	writel_relaxed(GPIOA_PUPDR_write, (g_ioremap_addr_gpioa + GPIOA_PUPDR_offset));
	
	/* set PG8 PU */
	GPIOG_PUPDR_write = readl_relaxed(g_ioremap_addr_gpiog + GPIOG_PUPDR_offset);
	GPIOG_PUPDR_write |= GPIOG_PUPDR_PUPDR8_0; 
	GPIOG_PUPDR_write &= ~(GPIOG_PUPDR_PUPDR8_1);

	writel_relaxed(GPIOG_PUPDR_write, (g_ioremap_addr_gpiog + GPIOG_PUPDR_offset));

	/* Disable the clocks after configuring the registers */
	clk_disable(clk_gpioa);
	clk_disable(clk_gpiog);

	count = of_get_child_count(dev->of_node);
	if (!count)
		return -EINVAL;

	dev_info(dev, "there are %d nodes\n", count);

	for_each_child_of_node(dev->of_node, child) {

		struct led_device *led_device;
		struct led_classdev *cdev;
		led_device = devm_kzalloc(dev, sizeof(*led_device), GFP_KERNEL);
		if (!led_device)
			return -ENOMEM;

		cdev = &led_device->cdev;

		led_device->base_gpioa = g_ioremap_addr_gpioa;
		led_device->base_gpiog = g_ioremap_addr_gpiog;

		of_property_read_string(child, "label", &cdev->name);

		if (strcmp(cdev->name,"ledgreen") == 0) {
			led_device->led_mask_set = GPIOA_PA10_CLEAR_BSRR_Mask;
			led_device->led_mask_clear = GPIOA_PA10_SET_BSRR_Mask;
			led_device->led_name ="ledgreen";
			led_device->cdev.default_trigger = "heartbeat";
			led_device->clk_gpioa = clk_gpioa;
			led_device->clk_gpiog = clk_gpiog;
		}
		else if (strcmp(cdev->name,"ledyellow") == 0) {
			led_device->led_mask_set = GPIOG_PG8_CLEAR_BSRR_Mask;
			led_device->led_mask_clear = GPIOG_PG8_SET_BSRR_Mask;
			led_device->led_name ="ledyellow";
			led_device->clk_gpioa = clk_gpioa;
			led_device->clk_gpiog = clk_gpiog;
		}
		else {
			dev_info(dev, "Bad device tree value\n");
			return -EINVAL;
		}

		/* Disable timer trigger until led is on */
		led_device->cdev.brightness = LED_OFF;
		led_device->cdev.brightness_set = led_control;

		ret_val = devm_led_classdev_register(dev, &led_device->cdev);
		if (ret_val) {
			dev_err(dev, "failed to register the led %s\n", cdev->name);
			of_node_put(child);
			return ret_val;
		}
	}
	
	dev_info(dev, "platform_probe exit\n");

	return 0;
}

static int ledclass_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "platform_remove enter\n");
	dev_info(&pdev->dev, "platform_remove exit\n");

	return 0;
}

static const struct of_device_id my_of_ids[] = {
	{ .compatible = "arrow,RGBclassleds"},
	{},
};

MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver led_platform_driver = {
	.probe = ledclass_probe,
	.remove = ledclass_remove,
	.driver = {
		.name = "RGBclassleds",
		.of_match_table = my_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(led_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alberto Liberal <aliberal@arroweurope.com>");
MODULE_DESCRIPTION("This is a driver that turns on/off RGB leds \
		           using the LED subsystem");

