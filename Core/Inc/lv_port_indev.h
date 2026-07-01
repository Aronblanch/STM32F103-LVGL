/**
 * @file lv_port_indev.h
 * @brief LVGL输入设备适配层 - FT5x16电容触摸屏 via I2C
 */
#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 初始化LVGL触摸输入设备
 * @note  必须在lv_init()和lv_port_disp_init()之后调用
 */
void lv_port_indev_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_INDEV_H */
