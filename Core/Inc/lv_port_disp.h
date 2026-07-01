/**
 * @file lv_port_disp.h
 * @brief LVGL显示驱动适配层 - ILI9488 320x480 RGB565 via FSMC
 */
#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 初始化LVGL显示驱动
 * @note  必须在lv_init()之后、UI创建之前调用
 */
void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_H */
