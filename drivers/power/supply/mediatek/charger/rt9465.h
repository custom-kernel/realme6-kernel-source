/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __RT9465_CHARGER_H
#define __RT9465_CHARGER_H

#define RT9465_SLAVE_ADDR	0x4B
#define RT9465_VERSION_E1	0x00
#define RT9465_VERSION_E2	0x01
#define RT9465_VERSION_E3	0x02
#define RT9465_VERSION_E4	0x03
#define RT9465_VERSION_E5	0x04

enum rt9465_reg_addr {
	RT9465_REG_CHG_CTRL0,
	RT9465_REG_CHG_CTRL1,
	RT9465_REG_CHG_CTRL2,
	RT9465_REG_CHG_CTRL3,
	RT9465_REG_CHG_CTRL4,
	RT9465_REG_CHG_CTRL5,
	RT9465_REG_CHG_CTRL6,
	RT9465_REG_CHG_CTRL7,
	RT9465_REG_CHG_CTRL8,
	RT9465_REG_CHG_CTRL9,
	RT9465_REG_CHG_CTRL10,
	RT9465_REG_CHG_CTRL12 = 0x0C,
	RT9465_REG_CHG_CTRL13,
	RT9465_REG_HIDDEN_CTRL2 = 0x12,
	RT9465_REG_HIDDEN_CTRL6 = 0x16,
	RT9465_REG_SYSTEM1 = 0x20,
	RT9465_REG_CHG_STATC = 0x30,
	RT9465_REG_CHG_FAULT,
	RT9465_REG_CHG_IRQ1,
	RT9465_REG_CHG_IRQ2,
	RT9465_REG_CHG_STATC_MASK = 0x40,
	RT9465_REG_CHG_FAULT_MASK,
	RT9465_REG_CHG_IRQ1_MASK,
	RT9465_REG_CHG_IRQ2_MASK,
	RT9465_REG_MAX,
};

/* =========================== */
/* RT9465 Parameter            */
/* =========================== */

/* uA */
#define RT9465_ICHG_NUM		20
#define RT9465_ICHG_MIN		600000
#define RT9465_ICHG_MAX		2500000
#define RT9465_ICHG_STEP	100000

/* uA */
#define RT9465_IEOC_NUM		11
#define RT9465_IEOC_MIN		600000
#define RT9465_IEOC_MAX		1600000
#define RT9465_IEOC_STEP	100000

/* uV */
#define RT9465_MIVR_NUM		128
#define RT9465_MIVR_MIN		3900000
#define RT9465_MIVR_MAX		13400000
#define RT9465_MIVR_STEP	100000

/* uV */
#define RT9465_BAT_VOREG_NUM	64
#define RT9465_BAT_VOREG_MIN	3800000
#define RT9465_BAT_VOREG_MAX	5060000
#define RT9465_BAT_VOREG_STEP	20000

/* ADC Temperature */
/* degree */
#define RT9465_ADC_RPT_NUM	15
#define RT9465_ADC_RPT_MIN	60
#define RT9465_ADC_RPT_MAX	116
#define RT9465_ADC_RPT_STEP	4

/* ========== CHG_CTRL0 0x00 ============ */
#define RT9465_SHIFT_RST	7
#define RT9465_MASK_RST		(1 << RT9465_SHIFT_RST)

/* ========== CHG_CTRL1 0x01 ============ */
#define RT9465_SHIFT_CHG_EN	7

#define RT9465_MASK_CHG_EN	(1 << RT9465_SHIFT_CHG_EN)

/* ========== CHG_CTRL3 0x03 ============ */
#define RT9465_SHIFT_BAT_VOREG	2

#define RT9465_MASK_BAT_VOREG	0xFC

/* ========== CHG_CTRL5 0x05 ============ */
#define RT9465_SHIFT_MIVR	1
#define RT9465_SHIFT_MIVR_EN	0

#define RT9465_MASK_MIVR	0xFE
#define RT9465_MASK_MIVR_EN	(1 << RT9465_SHIFT_MIVR_EN)

/* ========== CHG_CTRL6 0x06 ============ */
#define RT9465_SHIFT_ICHG	3

#define RT9465_MASK_ICHG	0xF8

/* ========== CHG_CTRL7 0x07 ============ */
#define RT9465_SHIFT_IEOC	4

#define RT9465_MASK_IEOC	0xF0

/* ========== CHG_CTRL8 0x08 ============ */
#define RT9465_SHIFT_TE_EN	7

#define RT9465_MASK_TE_EN	(1 << RT9465_SHIFT_TE_EN)

/* ========== CHG_CTRL9 0x09 ============ */
#define RT9465_SHIFT_TMR_EN	3
#define RT9465_SHIFT_WT_FC	5

#define RT9465_MASK_TMR_EN	(1 << RT9465_SHIFT_TMR_EN)
#define RT9465_MASK_WT_FC	0xE0

/* ========== CHG_CTRL10 0x0A ============ */
#define RT9465_SHIFT_WDT_EN	7

#define RT9465_MASK_WDT_EN	(1 << RT9465_SHIFT_WDT_EN)

/* ========== CHG_CTRL12 0x0C ============ */
#define RT9465_SHIFT_ADC_RPT	4

#define RT9465_MASK_ADC_RPT	0xF0

/* ========== SYSTEM1 0x20 ============ */
#define RT9465_SHIFT_CHG_STAT	6
#define RT9465_SHIFT_VERSION	1

#define RT9465_MASK_CHG_STAT	0xC0
#define RT9465_MASK_VERSION	0x0E

/* ========== STATC 0x30 ============ */
#define RT9465_SHIFT_CHG_MIVR	6

#define RT9465_MASK_CHG_MIVR	(1 << RT9465_SHIFT_CHG_MIVR)

#endif /* __RT9465_CHARGER_H */
