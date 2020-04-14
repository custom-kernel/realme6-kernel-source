/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __IMGSENSOR_PWR_CTRL_H__
#define __IMGSENSOR_PWR_CTRL_H__

#include <linux/mutex.h>

#include "imgsensor_sensor.h"
#include "imgsensor_cfg_table.h"
#include "imgsensor_common.h"

enum IMGSENSOR_HW_POWER_STATUS {
	IMGSENSOR_HW_POWER_STATUS_OFF,
	IMGSENSOR_HW_POWER_STATUS_ON
};

struct IMGSENSOR_HW_CUSTOM_POWER_INFO {
	enum IMGSENSOR_HW_PIN pin;
	enum IMGSENSOR_HW_ID id;
};

struct IMGSENSOR_HW_CFG {
	enum IMGSENSOR_SENSOR_IDX sensor_idx;
	enum IMGSENSOR_I2C_DEV i2c_dev;
	struct IMGSENSOR_HW_CUSTOM_POWER_INFO
					pwr_info[IMGSENSOR_HW_POWER_INFO_MAX];
};

struct IMGSENSOR_HW_POWER_INFO {
	enum IMGSENSOR_HW_PIN pin;
	enum IMGSENSOR_HW_PIN_STATE pin_state_on;
	u32 pin_on_delay;
	enum IMGSENSOR_HW_PIN_STATE pin_state_off;
	u32 pin_off_delay;
};

#ifdef ODM_HQ_EDIT
/* Lijian@ODM.Camera.Drv 20190912 MTK patch for mipi switch */
struct IMGSENSOR_HW_POWER_SEQ {
	char *name;
	struct IMGSENSOR_HW_POWER_INFO pwr_info[IMGSENSOR_HW_POWER_INFO_MAX];
	u32 _idx;
};
#else
struct IMGSENSOR_HW_POWER_SEQ {
	char *name;
	struct IMGSENSOR_HW_POWER_INFO pwr_info[IMGSENSOR_HW_POWER_INFO_MAX];
	u32 _idx;
};
#endif

struct IMGSENSOR_HW_DEVICE_COMMON {
	struct platform_device *pplatform_device;
	struct mutex            pinctrl_mutex;
};

struct IMGSENSOR_HW_DEVICE {
	enum IMGSENSOR_HW_ID id;
	void *pinstance;
	enum IMGSENSOR_RETURN (*init)(
			void *pinstance,
			struct IMGSENSOR_HW_DEVICE_COMMON *pcommon);
	enum IMGSENSOR_RETURN (*set)(
			void *pinstance,
			enum IMGSENSOR_SENSOR_IDX,
			enum IMGSENSOR_HW_PIN, enum IMGSENSOR_HW_PIN_STATE);
	enum IMGSENSOR_RETURN (*release)(void *pinstance);
	enum IMGSENSOR_RETURN (*dump)(void *pintance);
};

struct IMGSENSOR_HW_SENSOR_POWER {
	struct IMGSENSOR_HW_POWER_INFO *ppwr_info;
	enum   IMGSENSOR_HW_ID          id[IMGSENSOR_HW_PIN_MAX_NUM];
};

struct IMGSENSOR_HW {
	struct IMGSENSOR_HW_DEVICE_COMMON common;
	struct IMGSENSOR_HW_DEVICE       *pdev[IMGSENSOR_HW_ID_MAX_NUM];
	struct IMGSENSOR_HW_SENSOR_POWER
				sensor_pwr[IMGSENSOR_SENSOR_IDX_MAX_NUM];
	const char *enable_sensor_by_index[IMGSENSOR_SENSOR_IDX_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_hw_init(struct IMGSENSOR_HW *phw);
enum IMGSENSOR_RETURN imgsensor_hw_release_all(struct IMGSENSOR_HW *phw);
enum IMGSENSOR_RETURN imgsensor_hw_power(
		struct IMGSENSOR_HW *phw,
		struct IMGSENSOR_SENSOR *psensor,
		enum IMGSENSOR_HW_POWER_STATUS pwr_status);
enum IMGSENSOR_RETURN imgsensor_hw_dump(struct IMGSENSOR_HW *phw);

extern struct IMGSENSOR_HW_CFG imgsensor_custom_config[];
extern struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[];
extern struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[];
extern enum IMGSENSOR_RETURN (*hw_open[IMGSENSOR_HW_ID_MAX_NUM])
					(struct IMGSENSOR_HW_DEVICE **);

#endif

