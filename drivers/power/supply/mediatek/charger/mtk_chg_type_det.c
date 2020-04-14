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

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/charger_type.h>
#include <pmic.h>
#include <tcpm.h>

#include "mtk_charger_intf.h"



void __attribute__((weak)) fg_charger_in_handler(void)
{
pr_notice("%s not defined\n", __func__);
}
#ifdef VENDOR_EDIT
extern int oppo_ac_get_property(struct power_supply *psy,
enum power_supply_property psp,
union power_supply_propval *val);
extern int oppo_usb_get_property(struct power_supply *psy,
enum power_supply_property psp,
union power_supply_propval *val);
extern int oppo_battery_property_is_writeable(struct power_supply *psy,
enum power_supply_property psp);
extern int oppo_battery_set_property(struct power_supply *psy,
enum power_supply_property psp,
const union power_supply_propval *val);
extern int oppo_battery_get_property(struct power_supply *psy,
enum power_supply_property psp,
union power_supply_propval *val);
#endif

#ifdef ODM_HQ_EDIT
/*wangtao@ODM.HQ.BSP.CHG 2019/10/17 modify kernel error*/
extern int charger_manager_enable_chg_type_det(struct charger_consumer *consumer,
bool en);
#endif

enum charger_type g_chr_type;

struct chg_type_info {
struct device *dev;
struct charger_consumer *chg_consumer;
struct tcpc_device *tcpc_dev;
struct notifier_block pd_nb;
bool tcpc_kpoc;
/* Charger Detection */
struct mutex chgdet_lock;
bool chgdet_en;
atomic_t chgdet_cnt;
wait_queue_head_t waitq;
struct task_struct *chgdet_task;
struct workqueue_struct *pwr_off_wq;
struct work_struct pwr_off_work;
struct workqueue_struct *chg_in_wq;
struct work_struct chg_in_work;
bool ignore_usb;
bool plugin;
};

#ifdef CONFIG_FPGA_EARLY_PORTING
/*  FPGA */
int hw_charging_get_charger_type(void)
{
return STANDARD_HOST;
}

#else

/* EVB / Phone */
static const char * const mtk_chg_type_name[] = {
"Charger Unknown",
"Standard USB Host",
"Charging USB Host",
"Non-standard Charger",
"Standard Charger",
"Apple 2.1A Charger",
"Apple 1.0A Charger",
"Apple 0.5A Charger",
"Wireless Charger",
};

static void dump_charger_name(enum charger_type type)
{
switch (type) {
case CHARGER_UNKNOWN:
case STANDARD_HOST:
case CHARGING_HOST:
case NONSTANDARD_CHARGER:
case STANDARD_CHARGER:
case APPLE_2_1A_CHARGER:
case APPLE_1_0A_CHARGER:
case APPLE_0_5A_CHARGER:
#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/10, sjc Modify for charging */
	pr_err("!!! %s: charger type: %d, %s\n", __func__, type,
		mtk_chg_type_name[type]);
#else
	pr_err("%s: charger type: %d, %s\n", __func__, type,
		mtk_chg_type_name[type]);
#endif
	break;
default:
	pr_info("%s: charger type: %d, Not Defined!!!\n", __func__,
		type);
	break;
}
}

/* Power Supply */
struct mt_charger {
struct device *dev;
struct power_supply_desc chg_desc;
struct power_supply_config chg_cfg;
struct power_supply *chg_psy;
#if defined(VENDOR_EDIT) && !defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/10, sjc Modify for charging */
struct power_supply_desc ac_desc;
struct power_supply_config ac_cfg;
struct power_supply *ac_psy;
struct power_supply_desc usb_desc;
struct power_supply_config usb_cfg;
struct power_supply *usb_psy;
#endif /* VENDOR_EDIT && !CONFIG_OPPO_CHARGER_MT6370_TYPEC */
struct chg_type_info *cti;
bool chg_online; /* Has charger in or not */
enum charger_type chg_type;
};

static int mt_charger_online(struct mt_charger *mtk_chg)
{
int ret = 0;

#if defined(VENDOR_EDIT) && !defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/07/09, sjc Modify for charging */
int boot_mode = 0;

if (!mtk_chg->chg_online) {
	boot_mode = get_boot_mode();
	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		pr_notice("%s: Unplug Charger/USB\n", __func__);
		pr_notice("%s: system_state=%d\n", __func__,
			system_state);
		if (system_state != SYSTEM_POWER_OFF)
			kernel_power_off();
	}
}
#endif /* VENDOR_EDIT && !CONFIG_OPPO_CHARGER_MT6370_TYPEC */

return ret;
}
/************************************************/
/* Power Supply Functions
*************************************************/
#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/07/09, sjc Add for charging */
bool pmic_chrdet_status(void);
#endif
static int mt_charger_get_property(struct power_supply *psy,
enum power_supply_property psp, union power_supply_propval *val)
{
struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

switch (psp) {
case POWER_SUPPLY_PROP_ONLINE:
	val->intval = 0;
	/* Force to 1 in all charger type */
	if (mtk_chg->chg_type != CHARGER_UNKNOWN)
		val->intval = 1;
#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/07/09, sjc Add for charging */
	if ((get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
			|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT)
			&& (val->intval == 0)) {
		val->intval = pmic_chrdet_status();
		printk(KERN_ERR "%s: kpoc[%d]\n", __func__, val->intval);
	}
#endif /* VENDOR_EDIT && CONFIG_OPPO_CHARGER_MT6370_TYPEC */
	break;
#ifdef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/10/16 modified for bring up charging */		
case POWER_SUPPLY_PROP_CHARGE_TYPE:
	val->intval = mtk_chg->chg_type;
	break;
#endif
default:
	return -EINVAL;
}

return 0;
}


#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/15, sjc Add for charging */
extern bool oppo_chg_wake_update_work(void);
#endif /* VENDOR_EDIT && CONFIG_OPPO_CHARGER_MT6370_TYPEC */
static int mt_charger_set_property(struct power_supply *psy,
enum power_supply_property psp, const union power_supply_propval *val)
{
struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
struct chg_type_info *cti;
#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/15, sjc Add for charging */
static struct power_supply *battery_psy = NULL;
if (!battery_psy) {
	battery_psy = power_supply_get_by_name("battery");
}
#endif /* VENDOR_EDIT && CONFIG_OPPO_CHARGER_MT6370_TYPEC */

pr_info("%s\n", __func__);

if (!mtk_chg) {
	pr_notice("%s: no mtk chg data\n", __func__);
	return -EINVAL;
}

switch (psp) {
case POWER_SUPPLY_PROP_ONLINE:
	mtk_chg->chg_online = val->intval;
	mt_charger_online(mtk_chg);
	return 0;
case POWER_SUPPLY_PROP_CHARGE_TYPE:
	mtk_chg->chg_type = val->intval;
		g_chr_type = val->intval;
#ifdef ODM_WT_EDIT
/*Sidong.Zhao@ODM_WT.BSP.CHG 2019/11/4,for gm30 baseline upgrade*/
		oppo_chg_wake_update_work();
#endif /*ODM_WT_EDIT*/
#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/15, sjc Add for charging */
		if (battery_psy)
			power_supply_changed(battery_psy);
		oppo_chg_wake_update_work();
#endif /* VENDOR_EDIT && CONFIG_OPPO_CHARGER_MT6370_TYPEC */
		break;
	default:
		return -EINVAL;
	}

	dump_charger_name(mtk_chg->chg_type);

	cti = mtk_chg->cti;
	if (!cti->ignore_usb) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) ||
			(mtk_chg->chg_type == NONSTANDARD_CHARGER))
			mt_usb_connect();
		else
			mt_usb_disconnect();
	}

	queue_work(cti->chg_in_wq, &cti->chg_in_work);

#if defined(VENDOR_EDIT) && !defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/10, sjc Modify for charging */
	power_supply_changed(mtk_chg->ac_psy);
	power_supply_changed(mtk_chg->usb_psy);
#endif /* VENDOR_EDIT && !CONFIG_OPPO_CHARGER_MT6370_TYPEC */
	return 0;
}

#if defined(VENDOR_EDIT) && !defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/10, sjc Modify for charging */
static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		/* Reset to 0 if charger type is USB */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
} 

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif/* VENDOR_EDIT && !CONFIG_OPPO_CHARGER_MT6370_TYPEC */

static enum power_supply_property mt_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#if defined(VENDOR_EDIT) && !defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/10, sjc Modify for charging */
static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
    POWER_SUPPLY_PROP_OTG_SWITCH,
    POWER_SUPPLY_PROP_OTG_ONLINE,
    
};
#endif/* VENDOR_EDIT && !CONFIG_OPPO_CHARGER_MT6370_TYPEC */

static void tcpc_power_off_work_handler(struct work_struct *work)
{
	pr_info("%s\n", __func__);
	kernel_power_off();
}

static void charger_in_work_handler(struct work_struct *work)
{
	mtk_charger_int_handler();
	fg_charger_in_handler();
}

#ifdef CONFIG_TCPC_CLASS
static void plug_in_out_handler(struct chg_type_info *cti, bool en, bool ignore)
{
	mutex_lock(&cti->chgdet_lock);
	cti->chgdet_en = en;
	cti->ignore_usb = ignore;
	cti->plugin = en;
	atomic_inc(&cti->chgdet_cnt);
	wake_up_interruptible(&cti->waitq);
	mutex_unlock(&cti->chgdet_lock);
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct chg_type_info *cti = container_of(pnb,
		struct chg_type_info, pd_nb);
	int vbus = 0;

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			plug_in_out_handler(cti, true, false);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (cti->tcpc_kpoc) {
				vbus = battery_get_vbus();
				pr_info("%s KPOC Plug out, vbus = %d\n",
					__func__, vbus);
				#ifndef ODM_HQ_EDIT
				/* zhangchao@ODM.HQ.Charger 2019/12/10 modified for vooc power off charging */
				queue_work_on(cpumask_first(cpu_online_mask),
					      cti->pwr_off_wq,
					      &cti->pwr_off_work);
				#endif
				break;
			}
			pr_info("%s USB Plug out\n", __func__);
			plug_in_out_handler(cti, false, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			plug_in_out_handler(cti, true, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			plug_in_out_handler(cti, false, true);
		}
		break;
	}
	return NOTIFY_OK;
}
#endif

static int chgdet_task_threadfn(void *data)
{
	struct chg_type_info *cti = data;
	bool attach = false;
	int ret = 0;
#ifndef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/26 modified for power on */
	int i = 0;
	int max_wait_cnt = 40;

	for (i = 0; i < max_wait_cnt; i++) {
		msleep(500);

		cti->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
		if (!cti->tcpc_dev) {
			pr_info("%s get tcpc device type_c_port0 fail\n",
				__func__);
			continue;
		} else {
			cti->pd_nb.notifier_call = pd_tcp_notifier_call;
			ret = register_tcp_dev_notifier(cti->tcpc_dev,
				&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
			if (ret < 0) {
				pr_info("%s: register tcpc notifer fail\n",
					__func__);
			}
		}

		cti->chg_consumer = charger_manager_get_by_name(cti->dev,
			"charger_port1");
		if (!cti->chg_consumer) {
			pr_info("%s: get charger consumer device failed\n",
				__func__);
		}

		pr_info("%s: get tcpc and charger consumer done\n", __func__);
		break;
	}
#endif

	pr_info("%s: ++\n", __func__);
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(cti->waitq,
					     atomic_read(&cti->chgdet_cnt) > 0);
		if (ret < 0) {
			pr_info("%s: wait event been interrupted(%d)\n",
				__func__, ret);
			continue;
		}

		pm_stay_awake(cti->dev);
		mutex_lock(&cti->chgdet_lock);
		atomic_set(&cti->chgdet_cnt, 0);
		attach = cti->chgdet_en;
		mutex_unlock(&cti->chgdet_lock);

#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
		if (cti->chg_consumer)
			charger_manager_enable_chg_type_det(cti->chg_consumer,
							attach);
#else
		mtk_pmic_enable_chr_type_det(attach);
#endif
		pm_relax(cti->dev);
	}
	pr_info("%s: --\n", __func__);
	return 0;
}
#if defined(ODM_HQ_EDIT)
/* zhangchao@ODM.HQ.Charger 2019/10/16 modified for bring up charging */
static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;
	
	mt_chg = devm_kzalloc(&pdev->dev, sizeof(*mt_chg), GFP_KERNEL);

	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;

	mt_chg->chg_desc.name = "charger";
	mt_chg->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->chg_desc.properties = mt_charger_properties;
	mt_chg->chg_desc.num_properties = ARRAY_SIZE(mt_charger_properties);
	mt_chg->chg_desc.set_property = mt_charger_set_property;
	mt_chg->chg_desc.get_property = mt_charger_get_property;
	mt_chg->chg_cfg.drv_data = mt_chg;
#ifndef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/4 modified for bring up charging */
	mt_chg->ac_desc.name = "ac";
	mt_chg->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	mt_chg->ac_desc.properties = mt_ac_properties;
	mt_chg->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	mt_chg->ac_desc.get_property = mt_ac_get_property;
	mt_chg->ac_cfg.drv_data = mt_chg;

	mt_chg->usb_desc.name = "usb";
	mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	mt_chg->usb_desc.properties = mt_usb_properties;
	mt_chg->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	mt_chg->usb_desc.get_property = mt_usb_get_property;
	mt_chg->usb_cfg.drv_data = mt_chg;
#endif /* ODM_HQ_EDIT */
#ifndef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/4 modified for bring up charging */
	mt_chg->ac_psy = power_supply_register(&pdev->dev, &mt_chg->ac_desc,
		&mt_chg->ac_cfg);
	if (IS_ERR(mt_chg->ac_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->ac_psy));
		ret = PTR_ERR(mt_chg->ac_psy);
		goto err_ac_psy;
	}

	mt_chg->usb_psy = power_supply_register(&pdev->dev, &mt_chg->usb_desc,
		&mt_chg->usb_cfg);
	if (IS_ERR(mt_chg->usb_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->usb_psy));
		ret = PTR_ERR(mt_chg->usb_psy);
		goto err_usb_psy;
	}
#endif /* ODM_HQ_EDIT */
	cti = devm_kzalloc(&pdev->dev, sizeof(*cti), GFP_KERNEL);
	if (!cti) {
		ret = -ENOMEM;
		//goto err_no_mem;
	}
	cti->dev = &pdev->dev;

#ifdef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/26 modified for power on */
#ifdef CONFIG_TCPC_CLASS
	cti->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (cti->tcpc_dev == NULL) {
		pr_info("%s: tcpc device not ready, defer\n", __func__);
		ret = -EPROBE_DEFER;
		goto err_get_tcpc_dev;
	}
	cti->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc_dev,
		&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_info("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
#endif
/* wangtao@ODM.HQ.Charger 2019/12/3 modified usb not connect */
	mt_chg->chg_psy = power_supply_register(&pdev->dev,
		&mt_chg->chg_desc, &mt_chg->chg_cfg);
	if (IS_ERR(mt_chg->chg_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->chg_psy));
		ret = PTR_ERR(mt_chg->chg_psy);
		return ret;
	}

	cti->chg_consumer = charger_manager_get_by_name(cti->dev,
							"charger_port1");
	if (!cti->chg_consumer) {
		pr_info("%s: get charger consumer device failed\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
#endif

	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    ret == LOW_POWER_OFF_CHARGING_BOOT)
		cti->tcpc_kpoc = true;
	pr_info("%s KPOC(%d)\n", __func__, cti->tcpc_kpoc);

	/* Init Charger Detection */
	mutex_init(&cti->chgdet_lock);
	atomic_set(&cti->chgdet_cnt, 0);

	init_waitqueue_head(&cti->waitq);
	cti->chgdet_task = kthread_run(
				chgdet_task_threadfn, cti, "chgdet_thread");
	ret = PTR_ERR_OR_ZERO(cti->chgdet_task);
	if (ret < 0) {
		pr_info("%s: create chg det work fail\n", __func__);
		return ret;
	}

	/* Init power off work */
	cti->pwr_off_wq = create_singlethread_workqueue("tcpc_power_off");
	INIT_WORK(&cti->pwr_off_work, tcpc_power_off_work_handler);

	cti->chg_in_wq = create_singlethread_workqueue("charger_in");
	INIT_WORK(&cti->chg_in_work, charger_in_work_handler);

	mt_chg->cti = cti;
	platform_set_drvdata(pdev, mt_chg);
	device_init_wakeup(&pdev->dev, true);

	pr_info("%s done\n", __func__);
	return 0;

#ifdef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/26 modified for power on */
err_get_tcpc_dev:
	devm_kfree(&pdev->dev, cti);
	return ret;
#endif
//err_no_mem:
//	power_supply_unregister(mt_chg->usb_psy);

#ifndef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/4 modified for bring up charging */
err_usb_psy:
	power_supply_unregister(mt_chg->ac_psy);
err_ac_psy:
	power_supply_unregister(mt_chg->chg_psy);
	return ret;
#endif /* ODM_HQ_EDIT */
}

static int mt_charger_remove(struct platform_device *pdev)
{
	struct mt_charger *mt_charger = NULL;
	struct chg_type_info *cti = mt_charger->cti;

    mt_charger = platform_get_drvdata(pdev);
	power_supply_unregister(mt_charger->chg_psy);
#ifndef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/4 modified for bring up charging */
	power_supply_unregister(mt_charger->ac_psy);
	power_supply_unregister(mt_charger->usb_psy);
#endif /* ODM_HQ_EDIT */
	pr_info("%s\n", __func__);
	if (cti->chgdet_task) {
		kthread_stop(cti->chgdet_task);
		atomic_inc(&cti->chgdet_cnt);
		wake_up_interruptible(&cti->waitq);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt_charger_suspend(struct device *dev)
{
	/* struct mt_charger *mt_charger = dev_get_drvdata(dev); */
	return 0;
}

static int mt_charger_resume(struct device *dev)
{
	struct platform_device *pdev = NULL;
	struct mt_charger *mt_charger = NULL;
	/* zhangchao@ODM.HQ.Charger 2019/10/26 modified for dump problem */
	pdev = to_platform_device(dev);
	mt_charger = platform_get_drvdata(pdev);
	power_supply_changed(mt_charger->chg_psy);
#ifndef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/09/4 modified for bring up charging */
	power_supply_changed(mt_charger->ac_psy);
	power_supply_changed(mt_charger->usb_psy);
#endif /* ODM_HQ_EDIT */
	return 0;
}
#endif
#elif defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/06/10, sjc Modify for charging */
static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt_charger *mt_chg = NULL;

	mt_chg = devm_kzalloc(&pdev->dev, sizeof(struct mt_charger), GFP_KERNEL);
	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;

	mt_chg->chg_desc.name = "charger";
	mt_chg->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->chg_desc.properties = mt_charger_properties;
	mt_chg->chg_desc.num_properties = ARRAY_SIZE(mt_charger_properties);
	mt_chg->chg_desc.set_property = mt_charger_set_property;
	mt_chg->chg_desc.get_property = mt_charger_get_property;
	mt_chg->chg_cfg.drv_data = mt_chg;

	mt_chg->chg_psy = power_supply_register(&pdev->dev,
		&mt_chg->chg_desc, &mt_chg->chg_cfg);
	if (IS_ERR(mt_chg->chg_psy)) {
		dev_err(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->chg_psy));
		ret = PTR_ERR(mt_chg->chg_psy);
		return ret;
	}


	platform_set_drvdata(pdev, mt_chg);
	device_init_wakeup(&pdev->dev, 1);

	pr_info("%s\n", __func__);
	return 0;
}

static int mt_charger_remove(struct platform_device *pdev)
{
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);

	power_supply_unregister(mt_charger->chg_psy);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt_charger_suspend(struct device *dev)
{
	/* struct mt_charger *mt_charger = dev_get_drvdata(dev); */
	return 0;
}

static int mt_charger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);

	power_supply_changed(mt_charger->chg_psy);

	return 0;
}
#endif
#endif  /* VENDOR_EDIT && CONFIG_OPPO_CHARGER_MT6370_TYPEC */

static SIMPLE_DEV_PM_OPS(mt_charger_pm_ops, mt_charger_suspend,
	mt_charger_resume);

static const struct of_device_id mt_charger_match[] = {
	{ .compatible = "mediatek,mt-charger", },
	{ },
};
static struct platform_driver mt_charger_driver = {
	.probe = mt_charger_probe,
	.remove = mt_charger_remove,
	.driver = {
		.name = "mt-charger-det",
		.owner = THIS_MODULE,
		.pm = &mt_charger_pm_ops,
		.of_match_table = mt_charger_match,
	},
};

/* Legacy api to prevent build error */
#ifndef VENDOR_EDIT
/* Jianwei.Ye@BSP.CHG.Basic, 2019/09/10, Modify for charging */
bool upmu_is_chr_det(void)
{
	struct mt_charger *mtk_chg;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_online;
}
#else
bool upmu_is_chr_det(void)
{
	if (upmu_get_rgs_chrdet())
		return true;

	return false;
}
#endif
/* Legacy api to prevent build error */

//extern int get_oppo_short_check_fast_to_normal(void);
extern bool oppo_vooc_get_fastchg_started(void);

#ifdef VENDOR_EDIT
/* Yichun.Chen  PSW.BSP.CHG  2019-08-12  for aging issue */
extern int wakeup_fg_algo_atomic(unsigned int flow_state);
#define FG_INTR_CHARGER_OUT	4
#define FG_INTR_CHARGER_IN	8
static void notify_charger_status(bool cur_charger_exist)
{
	static bool pre_charger_exist = false;

	if (cur_charger_exist == true && pre_charger_exist == false) {
		printk("notify charger in\n");
		wakeup_fg_algo_atomic(FG_INTR_CHARGER_IN);
	} else if (cur_charger_exist == false && pre_charger_exist == true) {
		printk("notify charger out\n");
		wakeup_fg_algo_atomic(FG_INTR_CHARGER_OUT);
	}

	pre_charger_exist = cur_charger_exist;
}
#endif

bool pmic_chrdet_status(void)
{
#ifdef VENDOR_EDIT
//Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2018/01/12, add for fast chargering.
	if (oppo_vooc_get_fastchg_started()/* || get_oppo_short_check_fast_to_normal()*/) {
		notify_charger_status(true);
		return true;
	}
#endif /*VENDOR_EDIT*/
	if (upmu_is_chr_det()) {
#ifndef VENDOR_EDIT
		//Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/12/13, add for otg operation, to mislead to charger status.
		return true;
#else
		if (mt_usb_is_device()) {
			pr_err("[%s],Charger exist and USB is not host\n",__func__);
			notify_charger_status(true);
			return true;
		} else {
			pr_err("[%s],Charger exist but USB is host, now skip\n",__func__);
			notify_charger_status(false);
			return false;
		}
#endif /*VENDOR_EDIT*/
	}
	pr_err("%s: No charger\n", __func__);
#ifdef VENDOR_EDIT
/* Yichun.Chen  PSW.BSP.CHG  2019-08-11  for aging issue */
	notify_charger_status(false);
#endif
	return false;
}

enum charger_type mt_get_charger_type(void)
{
#ifdef ODM_HQ_EDIT
/* zhangchao@ODM.HQ.Charger 2019/10/25 modified for bring up charging */
	struct mt_charger *mtk_chg;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_type;
#else
#ifndef VENDOR_EDIT
/* Jianwei.Ye@BSP.CHG.Basic, 2019/09/10, Modify for charging */
	struct mt_charger *mtk_chg;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_type;
#else
	return g_chr_type;
#endif
#endif /*ODM_HQ_EDIT*/
}

bool mt_charger_plugin(void)
{
	struct mt_charger *mtk_chg;
	struct power_supply *psy = power_supply_get_by_name("charger");
	struct chg_type_info *cti;

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	cti = mtk_chg->cti;
	pr_info("%s plugin:%d\n", __func__, cti->plugin);

	return cti->plugin;
}

static s32 __init mt_charger_det_init(void)
{
	return platform_driver_register(&mt_charger_driver);
}

static void __exit mt_charger_det_exit(void)
{
	platform_driver_unregister(&mt_charger_driver);
}

subsys_initcall(mt_charger_det_init);
module_exit(mt_charger_det_exit);

MODULE_DESCRIPTION("mt-charger-detection");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");

#endif /* CONFIG_MTK_FPGA */
