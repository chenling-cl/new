// SPDX-License-Identifier: GPL-2.0
/*
 * X1830 SoC CGU driver
 * Copyright (c) 2019 Zhou Yanjie <zhouyanjie@zoho.com>
 */

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <dt-bindings/clock/x1830-cgu.h>
#include "cgu.h"
#include "pm.h"

/* CGU register offsets */
#define CGU_REG_CPCCR		0x00
#define CGU_REG_CPPCR       0X0c
#define CGU_REG_APLL		0x10
#define CGU_REG_MPLL		0x14
#define CGU_REG_VPLL		0xe0
#define CGU_REG_EPLL		0x58
#define CGU_REG_CLKGR0		0x20
#define CGU_REG_CLKGR1		0x28
#define CGU_REG_OPCR		0x24
#define CGU_REG_DDRCDR		0x2c
#define CGU_REG_MACCDR		0x54
#define CGU_REG_I2SCDR		0x60
#define CGU_REG_LPCDR		0x64
#define CGU_REG_MSC0CDR		0x68
#define CGU_REG_I2SCDR1		0x70
#define CGU_REG_SSICDR		0x74
#define CGU_REG_CIMCDR		0x7c
#define CGU_REG_PCMCDR		0x84
#define CGU_REG_MSC1CDR		0xa4

#define CGU_REG_CMP_INTR	0xb0
#define CGU_REG_CMP_INTRE	0xb4
#define CGU_REG_DRCG		0xd0
#define CGU_REG_CPCSR		0xd4
#define CGU_REG_PCMCDR1		0xe0
#define CGU_REG_MACPHYC		0xe8

/* bits within the OPCR register */
#define OPCR_SPENDN0		BIT(7)
#define OPCR_SPENDN1		BIT(6)

static struct ingenic_cgu *cgu;

static const s8 pll_od_encoding[64] = {
	0x0, 0x1, -1, 0x2, -1, -1, -1, 0x3,
	 -1,  -1, -1,  -1, -1, -1, -1, 0x4,
	 -1,  -1, -1,  -1, -1, -1, -1,  -1,
	 -1,  -1, -1,  -1, -1, -1, -1, 0x5,
	 -1,  -1, -1,  -1, -1, -1, -1,  -1,
	 -1,  -1, -1,  -1, -1, -1, -1,  -1,
	 -1,  -1, -1,  -1, -1, -1, -1,  -1,
	 -1,  -1, -1,  -1, -1, -1, -1, 0x6,
	};

static const struct ingenic_cgu_clk_info X1830_cgu_clocks[] = {

	/* External clocks */

	[X1830_CLK_EXCLK] = { "ext", CGU_CLK_EXT },
	[X1830_CLK_RTCLK] = { "rtc", CGU_CLK_EXT },

	/* PLLs */

	[X1830_CLK_APLL] = {
		"apll", CGU_CLK_PLL,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.pll = {
			.reg = CGU_REG_APLL,
			.rate_multiplier = 2,
			.m_shift = 20,
			.m_bits = 9,
			.m_offset = 1,
			.n_shift = 14,
			.n_bits = 6,
			.n_offset = 1,
			.od_shift = 11,
			.od_bits = 3,
			.od_max = 64,
			.od_encoding = pll_od_encoding,
			.bypass_reg = CGU_REG_CPPCR,	
			.bypass_bit = 30,
			.enable_bit = 0,
			.stable_bit = 3,
		},
	},

	[X1830_CLK_MPLL] = {
		"mpll", CGU_CLK_PLL,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.pll = {
			.reg = CGU_REG_MPLL,
			.rate_multiplier = 2,
			.m_shift = 20,
			.m_bits = 9,
			.m_offset = 1,
			.n_shift = 14,
			.n_bits = 6,
			.n_offset = 1,
			.od_shift = 11,
			.od_bits = 3,
			.od_max = 64,
			.od_encoding = pll_od_encoding,
			.bypass_reg = CGU_REG_CPPCR,
			.bypass_bit = 28,
			.enable_bit = 0,
			.stable_bit = 3,
		},
	},
	
	[X1830_CLK_VPLL] = {
		"vpll", CGU_CLK_PLL,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.pll = {
			.reg = CGU_REG_VPLL,
			.rate_multiplier = 2,
			.m_shift = 20,
			.m_bits = 9,
			.m_offset = 0,
			.n_shift = 14,
			.n_bits = 6,
			.n_offset = 0,
			.od_shift = 11,
			.od_bits = 3,
			.od_max = 64,
			.od_encoding = pll_od_encoding,
			.bypass_reg = CGU_REG_CPPCR,
			.bypass_bit = 24,
			.enable_bit = 0,
			.stable_bit = 3,
		},
	},

	[X1830_CLK_EPLL] = {
		"epll", CGU_CLK_PLL,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.pll = {
			.reg = CGU_REG_EPLL,
			.rate_multiplier = 2,
			.m_shift = 20,
			.m_bits = 9,
			.m_offset = 1,
			.n_shift = 14,
			.n_bits = 6,
			.n_offset = 1,
			.od_shift = 11,
			.od_bits = 3,
			.od_max = 64,
			.od_encoding = pll_od_encoding,
			.bypass_reg = CGU_REG_CPPCR,
			.bypass_bit = 26,
			.enable_bit = 0,
			.stable_bit = 3,
		},
	},


	/* Muxes & dividers */

	[X1830_CLK_SCLKA] = {
		"sclk_a", CGU_CLK_MUX,
		.parents = { -1, X1830_CLK_EXCLK, X1830_CLK_APLL, -1 },
		.mux = { CGU_REG_CPCCR, 30, 2 },
	},

	[X1830_CLK_CPUMUX] = {
		"cpu_mux", CGU_CLK_MUX,
		.parents = { -1, X1830_CLK_SCLKA, X1830_CLK_MPLL, -1 },
		.mux = { CGU_REG_CPCCR, 28, 2 },
	},

	[X1830_CLK_CPU] = {
		"cpu", CGU_CLK_DIV,
		.parents = { X1830_CLK_CPUMUX, -1, -1, -1 },
		.div = { CGU_REG_CPCCR, 0, 1, 4, 22, -1, -1 },
	},

	[X1830_CLK_L2CACHE] = {
		"l2cache", CGU_CLK_DIV,
		.parents = { X1830_CLK_CPUMUX, -1, -1, -1 },
		.div = { CGU_REG_CPCCR, 4, 1, 4, 22, -1, -1 },
	},

	[X1830_CLK_AHB0] = {
		"ahb0", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { -1, X1830_CLK_SCLKA, X1830_CLK_MPLL, -1 },
		.mux = { CGU_REG_CPCCR, 26, 2 },
		.div = { CGU_REG_CPCCR, 8, 1, 4, 21, -1, -1 },
	},

	[X1830_CLK_AHB2PMUX] = {
		"ahb2_apb_mux", CGU_CLK_MUX,
		.parents = { -1, X1830_CLK_SCLKA, X1830_CLK_MPLL, -1 },
		.mux = { CGU_REG_CPCCR, 24, 2 },
	},

	[X1830_CLK_AHB2] = {
		"ahb2", CGU_CLK_DIV,
		.parents = { X1830_CLK_AHB2PMUX, -1, -1, -1 },
		.div = { CGU_REG_CPCCR, 12, 1, 4, 20, -1, -1 },
	},

	[X1830_CLK_PCLK] = {
		"pclk", CGU_CLK_DIV,
		.parents = { X1830_CLK_AHB2PMUX, -1, -1, -1 },
		.div = { CGU_REG_CPCCR, 16, 1, 4, 20, -1, -1 },
	},

	[X1830_CLK_DDR] = {
		"ddr", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { -1, X1830_CLK_SCLKA, X1830_CLK_MPLL, -1 },
		.mux = { CGU_REG_DDRCDR, 30, 2 },
		.div = { CGU_REG_DDRCDR, 0, 1, 4, 29, 28, 27 },
		.gate = { CGU_REG_CLKGR0, 31 },
	},

	[X1830_CLK_MAC] = {
		"mac", CGU_CLK_MUX | CGU_CLK_DIV ,/*| CGU_CLK_GATE,*/
		.parents = { X1830_CLK_SCLKA, X1830_CLK_MPLL},
		.mux = { CGU_REG_MACCDR, 31, 2 },
		.div = { CGU_REG_MACCDR, 0, 1, 8, 29, 28, 27 },
		/*.gate = { CGU_REG_CLKGR0, 25 },*/
	},

	[X1830_CLK_MSCMUX] = {
		"msc_mux", CGU_CLK_MUX,
		.parents = { X1830_CLK_SCLKA, X1830_CLK_MPLL},
		.mux = { CGU_REG_MSC0CDR, 31, 2 },
	},

	[X1830_CLK_MSC0] = {
		"msc0", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { X1830_CLK_MSCMUX, -1, -1, -1 },
		.div = { CGU_REG_MSC0CDR, 0, 2, 8, 29, 28, 27 },
		.gate = { CGU_REG_CLKGR0, 4 },
	},

	[X1830_CLK_MSC1] = {
		"msc1", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { X1830_CLK_MSCMUX, -1, -1, -1 },
		.div = { CGU_REG_MSC1CDR, 0, 2, 8, 29, 28, 27 },
		.gate = { CGU_REG_CLKGR0, 5 },
	},

	[X1830_CLK_SSIPLL] = {
		"ssi_pll", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { X1830_CLK_SCLKA, X1830_CLK_MPLL, -1, -1 },
		.mux = { CGU_REG_SSICDR, 30, 1 },
		.div = { CGU_REG_SSICDR, 0, 1, 8, 29, 28, 27 },
	},

	[X1830_CLK_SSIPLL_DIV2] = {
		"ssi_pll_div2", CGU_CLK_FIXDIV,
		.parents = { X1830_CLK_SSIPLL },
		.fixdiv = { 2 },
	},

	[X1830_CLK_SSIMUX] = {
		"ssi_mux", CGU_CLK_MUX,
		.parents = { X1830_CLK_EXCLK, X1830_CLK_SSIPLL_DIV2, -1, -1 },
		.mux = { CGU_REG_SSICDR, 30, 1 },
	},

	/* Gate-only clocks */

	[X1830_CLK_SFC] = {
		"sfc", CGU_CLK_GATE,
		.parents = { X1830_CLK_SSIPLL, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 20 },
	},


	[X1830_CLK_EMC] = {
		"emc", CGU_CLK_GATE,
		.parents = { X1830_CLK_AHB2, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 0 },
	},

	[X1830_CLK_EFUSE] = {
		"efuse", CGU_CLK_GATE,
		.parents = { X1830_CLK_AHB2, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 1 },
	},

	[X1830_CLK_OTG] = {
		"otg", CGU_CLK_GATE,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 3 },
	},

	[X1830_CLK_SSI0] = {
		"ssi0", CGU_CLK_GATE,
		.parents = { X1830_CLK_SSIMUX, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 6 },
	},	

	[X1830_CLK_I2C0] = {
		"i2c0", CGU_CLK_GATE,
		.parents = { X1830_CLK_PCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 7 },
	},

	[X1830_CLK_I2C1] = {
		"i2c1", CGU_CLK_GATE,
		.parents = { X1830_CLK_PCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 8 },
	},

	[X1830_CLK_I2C2] = {
		"i2c2", CGU_CLK_GATE,
		.parents = { X1830_CLK_PCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 9 },
	},

	[X1830_CLK_UART0] = {
		"uart0", CGU_CLK_GATE,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 14 },
	},

	[X1830_CLK_UART1] = {
		"uart1", CGU_CLK_GATE,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 15 },
	},

	[X1830_CLK_SSI] = {
		"ssi1", CGU_CLK_GATE,
		.parents = { X1830_CLK_SSIMUX, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 19 },
	},

	[X1830_CLK_PDMA] = {
		"pdma", CGU_CLK_GATE,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 21 },
	},

	[X1830_CLK_TCU] = {
		"tcu", CGU_CLK_GATE,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR0, 30 },
	},

	[X1830_CLK_DTRNG] = {
		"dtrng", CGU_CLK_GATE,
		.parents = { X1830_CLK_PCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR1, 1 },
	},

	[X1830_CLK_OST] = {
		"ost", CGU_CLK_GATE,
		.parents = { X1830_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR1, 11 },
	},
};

static void __init X1830_cgu_init(struct device_node *np)
{
	int retval;

	cgu = ingenic_cgu_new(X1830_cgu_clocks,
			      ARRAY_SIZE(X1830_cgu_clocks), np);
	if (!cgu) {
		pr_err("%s: failed to initialise CGU\n", __func__);
		return;
	}

	retval = ingenic_cgu_register_clocks(cgu);
	if (retval) {
		pr_err("%s: failed to register CGU Clocks\n", __func__);
		return;
	}

	ingenic_cgu_register_syscore_ops(cgu);
}
CLK_OF_DECLARE(X1830_cgu, "ingenic,X1830-cgu", X1830_cgu_init);
