/**
 * @file lv_port_indev.c
 * @brief LVGL输入设备适配层 - 对接FT5x16电容触摸驱动
 */
#include "lv_port_indev.h"
#include "touch.h"

static lv_indev_t *indev_touchpad;

/**
 * @brief 触摸屏读取回调
 * @note  FT5x16_Scan()内部已做坐标校准映射，输出直接是LCD坐标
 */
static void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;

    if(FT5x16_Scan(&x, &y)) {
        data->point.x = (int32_t)x;
        data->point.y = (int32_t)y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_indev_init(void)
{
    /* 创建输入设备 (LVGL v9 API) */
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read_cb);
}
