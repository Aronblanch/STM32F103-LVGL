/**
 ******************************************************************************
 * @file    touch.h
 * @brief   FT5x16 电容触摸驱动头文件
 * @note    基于 HAL 硬件 I2C1 (PB6=SCL, PB7=SDA)
 *          触摸中断 PC1 (EXTI1 下降沿)
 ******************************************************************************
 */

#ifndef __TOUCH_H
#define __TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ============================================================================
 */
/*  FT5x16 I2C 地址与关键寄存器 */
/* ============================================================================
 */

/* FT5x16 7位 I2C 设备地址 (数据手册: 地址引脚拉低=0x38, 拉高=0x39) */
#define FT5X16_I2C_ADDR 0x38

/* FT5x16 关键寄存器地址 (数据手册 Table 6-1) */
#define FT5X16_REG_DEVIDE_MODE 0x00  /* 设备模式 */
#define FT5X16_REG_GEST_ID 0x01      /* 手势ID */
#define FT5X16_REG_TD_STATUS 0x02    /* 触摸点数 (0~5) */
#define FT5X16_REG_TOUCH1_XH 0x03    /* 第1点 X 高4位 + 触摸标志 */
#define FT5X16_REG_TOUCH1_XL 0x04    /* 第1点 X 低8位 */
#define FT5X16_REG_TOUCH1_YH 0x05    /* 第1点 Y 高4位 + 触摸标志 */
#define FT5X16_REG_TOUCH1_YL 0x06    /* 第1点 Y 低8位 */
#define FT5X16_REG_TH_GROUP 0x80     /* 触摸阈值 */
#define FT5X16_REG_PERIODACTIVE 0x88 /* 扫描周期 */
#define FT5X16_REG_CHIP_ID 0xA3      /* 芯片ID (高字节) */
#define FT5X16_REG_VENDOR_ID 0xA8    /* 厂商ID */

/* I2C 操作超时 (ms) */
#define FT5X16_I2C_TIMEOUT 50

/* ============================================================================
 */
/*  触摸坐标校准参数 */
/*                                                                              */
/*  FT5x16 输出的触摸坐标范围可能与 LCD 分辨率不同 */
/*  典型情况: FT5x16 输出 0~4095 的12位ADC值，需映射到 320×480 的LCD像素 */
/*  此外触摸屏与LCD的安装方向可能存在旋转/镜像差异 */
/*                                                                              */
/*  校准参数说明: */
/*  - x/y_offset: 触摸坐标偏移量 */
/*  - x/y_scale:  缩放因子 (定点数, 256表示1:1) */
/*  - swap_xy:    是否交换 X/Y 轴 */
/*  - x/y_mirror: 是否镜像 X/Y 轴 */
/* ============================================================================
 */

typedef struct {
  int16_t x_offset; /* X轴偏移 (触摸值 - 偏移 = 实际值) */
  int16_t y_offset; /* Y轴偏移 */
  uint16_t x_scale; /* X轴缩放: lcd_x = (touch_x * x_scale) >> 8 */
  uint16_t y_scale; /* Y轴缩放: lcd_y = (touch_y * y_scale) >> 8 */
  uint8_t swap_xy;  /* 1=交换X/Y轴, 0=不交换 */
  uint8_t x_mirror; /* 1=镜像X轴, 0=不镜像 */
  uint8_t y_mirror; /* 1=镜像Y轴, 0=不镜像 */
} Touch_Calib_t;

/* ============================================================================
 */
/*  函数声明 */
/* ============================================================================
 */

/**
 * @brief  FT5x16 初始化，读取芯片ID验证 I2C 通信
 * @retval 0=成功, 1=失败
 */
uint8_t FT5x16_Init(void);

/**
 * @brief  触摸扫描，读取当前触摸坐标
 * @param  x: 输出X坐标指针 (LCD像素坐标)
 * @param  y: 输出Y坐标指针 (LCD像素坐标)
 * @retval 1=有触摸, 0=无触摸
 */
uint8_t FT5x16_Scan(uint16_t *x, uint16_t *y);

/**
 * @brief  设置触摸校准参数
 * @param  calib: 校准参数结构体指针
 */
void FT5x16_SetCalibration(const Touch_Calib_t *calib);

/**
 * @brief  获取当前校准参数
 * @retval 校准参数结构体指针
 */
const Touch_Calib_t *FT5x16_GetCalibration(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_H */
