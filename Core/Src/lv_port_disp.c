/**
 * @file lv_port_disp.c
 * @brief LVGL显示驱动适配层 - 对接ILI9488 FSMC 16位并口驱动
 */
#include "lv_port_disp.h"
#include "lcd.h"

/* ---------- 显示缓冲区配置 ---------- */
/* 缓冲区放在外部SRAM (IS62WV51216 @ 0x68000000) */
#define DISP_HOR_RES    320
#define DISP_VER_RES    480
#define DISP_BUF_LINES  80   /* 每次刷新80行，6次覆盖全屏 */
#define DISP_BUF_SIZE   (DISP_HOR_RES * DISP_BUF_LINES * sizeof(uint16_t))  /* 51,200 bytes */

static uint8_t *buf1 = (uint8_t *)0x68000000;  /* 缓冲区1: 0x68000000 ~ 0x6800C7FF */
static uint8_t *buf2 = (uint8_t *)0x6800C800;  /* 缓冲区2: 0x6800C800 ~ 0x68018FFF */

static lv_display_t *disp;

/* ---------- Flush回调 ---------- */
/**
 * @brief 将LVGL渲染缓冲区写入LCD
 * @note  通过FSMC逐像素写入ILI9488
 */
static void disp_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t *color_p = (uint16_t *)px_map;

    /* 设置LCD显示窗口 */
    LCD_SetWindows(area->x1, area->y1, area->x2, area->y2);

    /* 逐像素写入 */
    uint32_t size = (uint32_t)(area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    for(uint32_t i = 0; i < size; i++) {
        LCD_WriteData(*color_p++);
    }

    /* 通知LVGL刷新完成 */
    lv_display_flush_ready(display);
}

/* ---------- 公开接口 ---------- */
void lv_port_disp_init(void)
{
    /* 创建显示设备 (LVGL v9 API) */
    disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);

    /* 注册flush回调 */
    lv_display_set_flush_cb(disp, disp_flush_cb);

    /* 设置双缓冲，部分渲染模式 */
    lv_display_set_buffers(disp, buf1, buf2, DISP_BUF_SIZE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}
