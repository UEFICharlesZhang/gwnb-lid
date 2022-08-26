// SPDX-License-Identifier: GPL-2.0
/*
 * Power supply driver for the goldfish emulator
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2012 Intel, Inc.
 * Copyright (C) 2013 Intel, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/acpi.h>
#include <linux/delay.h>

#include <linux/input.h>

struct gw_nb_battery_data {
	void __iomem *reg_base;
	int irq;
	spinlock_t lock;

	struct input_dev *input;
	struct device *dev;
};

#define POWER_STATUS 0xB0
#define AC_STATUS 1U << 0
#define BAT_STATUS 1U << 1

#define BATTERY_STATUS 0x3C

#define BATTERY_PERCENT 0x21
#define BATTERY_VOLTAGE_L 0x2A
#define BATTERY_VOLTAGE_H 0x2B

#define BATTERY_TEMP_L 0x28
#define BATTERY_TEMP_H 0x29

#define BATTERY_CUR_NOW_L 0x2C
#define BATTERY_CUR_NOW_H 0x2D

#define BATTERY_CUR_AVG_L 0x2E
#define BATTERY_CUR_AVG_H 0x2F

#define BATTERY_FULL_CAP_L 0x24
#define BATTERY_FULL_CAP_H 0x25

#define BATTERY_CUR_CAP_L 0x26
#define BATTERY_CUR_CAP_H 0x27

#define BATTERY_DES_FULL_CAP_L 0x38
#define BATTERY_DES_FULL_CAP_H 0x39

#define BATTERY_DES_FULL_CAP_L 0x38
#define BATTERY_DES_FULL_CAP_H 0x39

#define BATTERY_DES_VOLTAGE_L 0x3A
#define BATTERY_DES_VOLTAGE_H 0x3B

#define BATTERY_SN_L 0x3E
#define BATTERY_SN_H 0x3F


#define GPIO_0_INT 42
#define GPIO_1_INT 43

#define LPC_BASE_ADDR 0x20000000

#define I8042_COMMAND_REG 0x66UL
#define I8042_DATA_REG 0x62UL

void __iomem * lpc_base;
void __iomem * gpio_iobase;

//Wait till EC I/P buffer is free
static int 
EcIbFree (void
  )
{
  static int  Status;
  do {
    Status = readb (lpc_base + I8042_COMMAND_REG);
    // DEBUG ((EFI_D_INFO, "%a(), Status = %08x\n", __FUNCTION__, Status));
  } while (Status & 2);
  return 0;
}

//Wait till EC O/P buffer is full
static int 
EcObFull (void
  )
{
  static int  Status;
  do {
    Status = readb (lpc_base + I8042_COMMAND_REG);
    // DEBUG ((EFI_D_INFO, "%a(), Status = %08x\n", __FUNCTION__, Status));
  } while(!(Status & 1));
  return 0;
}

//Send EC command
static int 
EcWriteCmd (
  int  Cmd
  )
{
  EcIbFree ();
  writeb (Cmd,lpc_base + I8042_COMMAND_REG);
  return 0;
}

//Write Data from EC data port
static int 
EcWriteData (
  int  Data
  )
{
  EcIbFree ();
  writeb (Data,lpc_base + I8042_DATA_REG);
  return 0;
}

//Read Data from EC data Port
static int 
EcReadData (
   int  *PData
  )
{
  //暂时修改
  *PData = readb (lpc_base + I8042_DATA_REG);
  EcObFull ();
  *PData = readb (lpc_base + I8042_DATA_REG);
  return 0;
}

//Read Data from EC Memory from location pointed by Index
static int 
EcReadMem (
   int  Index,
   int  *Data
  )
{
  static int  Cmd;
  Cmd = 0x80;
  EcWriteCmd (Cmd);
  EcWriteData (Index);
  EcReadData (Data);
  return 0;
}

static int gw_ec_read(int offset)
{
	int tmp;
	// printk(KERN_ERR "gw_ec_read\n");
	EcReadMem(offset,&tmp);
	// printk(KERN_ERR "gw_ec_read value:0x%x\n",tmp);
	return tmp;
}

static void gwnb_lid_poll(struct input_dev *input)
{
	bool lidstatus;
	printk("[%s] :%d, recieve an SCI\n", __func__, __LINE__);

	lidstatus = gw_ec_read(0x46) & BIT(0);
	printk(KERN_ERR "gw_ec_read lidstatus:0x%x\n", lidstatus);

	if (lidstatus)
	{
		printk("lid close\n");
		input_report_switch(input,
							SW_LID, 1);
		input_sync(input);
	}
	else
	{
		printk("lid open\n");
		input_report_switch(input,
							SW_LID, 0);
		input_sync(
			input);
	}
}

static int goldfish_buttons_init(struct gw_nb_battery_data *data)
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


	ret = input_setup_polling(data->input, gwnb_lid_poll);
	if (ret) {
		dev_err(data->dev, "could not set up polling mode, %d\n", ret);
		return ret;
	}

	input_set_poll_interval(data->input, 100);

	ret = input_register_device(data->input);
	if (ret)
		return -ENODEV;

	device_init_wakeup(data->dev, 1);
	return 0;
}
static int gwnb_lid_probe(struct platform_device *pdev)
{
	struct gw_nb_battery_data *data;
	u32 temp;

	printk(KERN_ERR "GW NB Battery probe\n");

	data = devm_kzalloc(&pdev->dev, sizeof(struct gw_nb_battery_data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	spin_lock_init(&data->lock);

	data->dev = &pdev->dev;

	goldfish_buttons_init(data);

	platform_set_drvdata(pdev, data);

	lpc_base = ioremap(LPC_BASE_ADDR, 0x100);
	gpio_iobase = ioremap(0x28004000, 0x100);

    //Charles set SCI gpio (GPIOA 07)
	//This should be done by BIOS not driver.
	temp = readl(gpio_iobase + 0x00);
	temp &= (~BIT(7));
	writel(temp,gpio_iobase + 0x00);

	temp = readl(gpio_iobase + 0x1C);
	temp &= (~BIT(7));
	writel(temp,gpio_iobase + 0x1C);

	temp = readl(gpio_iobase + 0x20);
	temp |= BIT(7);
	writel(temp,gpio_iobase + 0x20);

	temp = readl(gpio_iobase + 0x24);
	temp &= (~BIT(7));
	writel(temp,gpio_iobase + 0x24);

	temp = readl(gpio_iobase + 0x18);
	temp |= BIT(7);
	writel(temp,gpio_iobase + 0x18);

	EcWriteCmd (0x86);

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
			.name = "gw-nb-lid",
			.acpi_match_table = ACPI_PTR(
				gwnb_lid_acpi_match),
		},
};
module_platform_driver(gwnb_lid_device);


MODULE_AUTHOR("zhangshuzhen@greatwall.com.cn");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery driver for greatwall ft notebooks");
