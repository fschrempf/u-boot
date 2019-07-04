// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Kontron Electronics GmbH
 */

#include <common.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
#ifdef CONFIG_OPTEE
	/* rom_pointer[1] contains the size of TEE occupies */
	if (rom_pointer[1])
		gd->ram_size = PHYS_SDRAM_SIZE - rom_pointer[1];
	else
#endif
		gd->ram_size = PHYS_SDRAM_SIZE;

	return 0;
}

#ifndef CONFIG_SPL_BUILD
int board_early_init_f(void)
{
	return 0;
}
#endif

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "Kontron N8000 SMARC");
	env_set("board_rev", "0");
#endif
	return 0;
}


