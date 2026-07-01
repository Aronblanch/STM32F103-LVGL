/**
  ******************************************************************************
  * @file    lcd.h
  * @brief   ILI9488 3.5" TFT LCD 驱动头文件
  * @note    基于 FSMC 16位并口驱动，RGB565 颜色格式
  *          片选 NE4(PG12), 命令/数据选择 A10(PG0)
  ******************************************************************************
  */

#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ============================================================================ */
/*  FSMC 地址计算（任务1）                                                       */
/*                                                                              */
/*  Bank1 Region4 基地址: 0x6C000000                                            */
/*  16位数据宽度下，外部地址线 A_out[x] = HADDR[x+1]（右移1位对齐）              */
/*  RS 接 A10: 命令时 A10=0, 数据时 A10=1                                       */
/*  外部 A10=1 → HADDR bit11=1 → 偏移 = 1<<11 = 0x800                          */
/* ============================================================================ */
#define LCD_BASE        0x6C000000UL    /* Bank1 Region4 基地址 (NE4) */
#define LCD_CMD_ADDR    (LCD_BASE + 0x000)  /* A10=0: 写命令地址 */
#define LCD_DATA_ADDR   (LCD_BASE + 0x800)  /* A10=1: 写数据地址 */

/* 命令/数据指针宏 — 直接通过 FSMC 内存映射读写 */
#define LCD_CMD         ((__IO uint16_t *)LCD_CMD_ADDR)
#define LCD_DATA        ((__IO uint16_t *)LCD_DATA_ADDR)

/* ============================================================================ */
/*  屏幕尺寸                                                                    */
/* ============================================================================ */
#define LCD_WIDTH       320
#define LCD_HEIGHT      480

/* ============================================================================ */
/*  ILI9488 关键寄存器命令定义                                                   */
/* ============================================================================ */
#define ILI9488_SWRESET     0x01    /* 软件复位 */
#define ILI9488_SLEEPOUT    0x11    /* 退出睡眠模式 */
#define ILI9488_GAMMA_SET   0x13    /* 普通 Gamma 选择 */
#define ILI9488_DISPON      0x29    /* 开启显示 */
#define ILI9488_DISPOFF     0x28    /* 关闭显示 */
#define ILI9488_CASET       0x2A    /* 列地址设置 */
#define ILI9488_PASET       0x2B    /* 行地址设置 */
#define ILI9488_RAMWR       0x2C    /* 写显示内存 */
#define ILI9488_RAMRD       0x2E    /* 读显示内存 */
#define ILI9488_MADCTL      0x36    /* 存储器访问控制 */
#define ILI9488_PIXFMT      0x3A    /* 像素格式设置 */
#define ILI9488_FRMCTR1     0xB1    /* 帧率控制(正常模式) */
#define ILI9488_FRMCTR2     0xB2    /* 帧率控制(空闲模式) */
#define ILI9488_FRMCTR3     0xB3    /* 帧率控制(部分模式) */
#define ILI9488_INVCTR      0xB4    /* 反转控制 */
#define ILI9488_DFUNCTR     0xB6    /* 显示功能控制 */
#define ILI9488_PWCTR1      0xC0    /* 电源控制1 */
#define ILI9488_PWCTR2      0xC1    /* 电源控制2 */
#define ILI9488_VMCTR1      0xC5    /* VCOM 控制 */
#define ILI9488_PGAMCTRL    0xE0    /* 正极性 Gamma 校正 */
#define ILI9488_NGAMCTRL    0xE1    /* 负极性 Gamma 校正 */
#define ILI9488_IFCTL       0xB6    /* 接口控制 */
#define ILI9488_IFMODE      0xB0    /* 接口模式控制 */
#define ILI9488_SETIMAGE    0xE9    /* 设置图像模式 */
#define ILI9488_ADAPTIVE    0xF7    /* 自适应更新 */
#define ILI9488_INVON       0x21    /* 颜色反转开启 */

/* ============================================================================ */
/*  RGB565 颜色常量（16位: R5-G6-B5）                                           */
/*  注意: ILI9488 内部为 RGB666，RGB565 的红蓝5位会被零填充到6位                  */
/*  若实际显示颜色偏暗或反转，可在初始化中启用 0x21(INVON) 或调整 BGR 位          */
/* ============================================================================ */
#define COLOR_WHITE     0xFFFF
#define COLOR_BLACK     0x0000
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x8410

/* 从 RGB 分量合成 RGB565 颜色值 */
#define RGB565(r, g, b)  (((uint16_t)(r & 0xF8) << 8) | \
                          ((uint16_t)(g & 0xFC) << 3) | \
                          ((uint16_t)(b & 0xF8) >> 3))

/* ============================================================================ */
/*  函数声明                                                                    */
/* ============================================================================ */
void LCD_Init(void);
void LCD_WriteCmd(uint16_t cmd);
void LCD_WriteData(uint16_t data);
uint16_t LCD_ReadData(void);
void LCD_SetWindows(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_Clear(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */
