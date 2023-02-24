// SPDX-License-Identifier: GPL-2.0
/*
 * Lid driver for the Great Wall FT Notebooks.
 *
 * Author: Charles Zhang <zhangshuzhen@greatwall.com.cn>
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/acpi.h>

#include <linux/input.h>

struct gw_nb_lib {
	spinlock_t lock;

	struct input_dev *input;
	struct device *dev;
};


static int gwnb_buttons_init(struct gw_nb_lib *data)
{
	int ret;

	data->input = devm_input_allocate_device(data->dev);
	if (data->input == NULL)
		return -ENOMEM;

	data->input->name = "gwnb-lid";
	data->input->phys = "gwnb-lid/input0";
	data->input->id.bustype = BUS_VIRTUAL;
	data->input->id.vendor  = 0x6868;
	data->input->id.product = 0x3232;
	data->input->dev.parent = data->dev;

	input_set_capability(data->input, EV_SW, SW_LID);

	ret = input_register_device(data->input);
	if (ret)
		return -ENODEV;

	device_init_wakeup(data->dev, 1);
	return 0;
}
static int gwnb_lid_probe(struct platform_device *pdev)
{
	struct gw_nb_lib *data;
	printk(KERN_ERR "GW NB Lid probe\n");

	data = devm_kzalloc(&pdev->dev, sizeof(struct gw_nb_lib), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	spin_lock_init(&data->lock);

	data->dev = &pdev->dev;

	gwnb_buttons_init(data);

	platform_set_drvdata(pdev, data);

	return 0;
}

static int gwnb_lid_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id gwnb_lid_acpi_match[] = {
	{ "FTEC0001", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, gwnb_lid_acpi_match);
#endif

static struct platform_driver
	gwnb_lid_device = {
		.probe = gwnb_lid_probe,
		.remove = gwnb_lid_remove,
		.driver = {
			.name = "gwnb-lid",
			.acpi_match_table = ACPI_PTR(
				gwnb_lid_acpi_match),
		},
};
module_platform_driver(gwnb_lid_device);


MODULE_AUTHOR("Charles Zhang");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lid driver for the Great Wall FT Notebooks.");
MODULE_ALIAS("gwnb-lid");