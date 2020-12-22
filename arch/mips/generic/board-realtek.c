// SPDX-License-Identifier: GPL-2.0-only

#include <asm/setup.h>
#include <asm/machine.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/printk.h>

static int __init realtek_init(void)
{
	/* uart0 */
	setup_8250_early_printk_port(0xb8002000, 2, 0);

	return 0;
}

early_initcall(realtek_init);

