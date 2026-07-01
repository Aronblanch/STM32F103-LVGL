/**
 ******************************************************************************
 * @file    IS62WV512.h
 * @brief   IS62WV51216BLL-55TLI SRAM 驱动头文件
 *          - 512K x 16 bit (8Mbit), 2.5V-3.6V, 55ns
 *          - FSMC Bank3 (NE3), 16-bit 总线模式
 ******************************************************************************
 */
#ifndef __IS62WV512_H
#define __IS62WV512_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* ------------------------------------------------------------------------- */
/*  硬件连接配置宏                                                           */
/* ------------------------------------------------------------------------- */

/**
 * @brief  SRAM FSMC 基地址
 *
 * hsram2 使用 FSMC_NORSRAM_BANK3 (NE3片选)，
 * Bank1 四个子 Bank 地址映射:
 *   Sub-bank 1: 0x60000000 - 0x63FFFFFF (NE1)
 *   Sub-bank 2: 0x64000000 - 0x67FFFFFF (NE2)
 *   Sub-bank 3: 0x68000000 - 0x6BFFFFFF (NE3)  <-- 当前使用
 *   Sub-bank 4: 0x6C000000 - 0x6FFFFFFF (NE4)
 */
#define SRAM_BASE_ADDR 0x68000000U

/**
 * @brief  SRAM 容量 (字节)
 *         IS62WV51216BLL: 512K x 16bit = 1,048,576 字节 (1MB)
 */
#define SRAM_SIZE_BYTES (1024U * 1024U)

/**
 * @brief  SRAM 容量 (半字，16-bit)
 */
#define SRAM_SIZE_HALFWORDS (SRAM_SIZE_BYTES / 2U)

/**
 * @brief  字节控制模式选择
 *
 * SRAM_NBL_STANDARD: NBL0(PE0) -> LB, NBL1(PE1) -> UB (标准 FSMC 字节使能接法)
 *   - FSMC 硬件在访问偶地址时自动拉低 NBL0 (使能低8位 D[7:0])
 *   - FSMC 硬件在访问奇地址时自动拉低 NBL1 (使能高8位 D[15:8])
 *
 * SRAM_NBL_ON_ADDRESS: LB/UB 接在 FSMC_A0/A1 地址线上 (非标准接法)
 *   - 需要软件手动控制地址线来选择字节
 *
 * 当前默认: SRAM_NBL_STANDARD (根据 CubeMX 2ByteEnable2 配置)
 *
 * 注意: 如果硬件实际接法不同，请取消注释下方宏定义:
 * #define SRAM_NBL_ON_ADDRESS
 */
#define SRAM_NBL_STANDARD 1U

#ifdef SRAM_NBL_ON_ADDRESS
#undef SRAM_NBL_STANDARD
#define SRAM_NBL_STANDARD 0U
#endif

/* ------------------------------------------------------------------------- */
/*  FSMC 时序参数 (基于 HCLK = 72MHz, tHCLK = 13.89ns)                     */
/*  参考: IS62WV51216BLL-55TLI 规格书 (55ns 版本)                           */
/* ------------------------------------------------------------------------- */

/**
 * 地址建立时间 (Address Setup Time)
 * 公式: (ADDSET + 1) x tHCLK >= tACS (55ns)
 * 计算: (4 + 1) x 13.89ns = 69.4ns >= 55ns  [PASS, 裕量 +14.4ns]
 */
#define SRAM_ADDR_SETUP_TIME 4U

/**
 * 数据建立时间 (Data Setup Time)
 * 公式: (DATAST + 1) x tHCLK >= tPWE (40ns)
 * 计算: (4 + 1) x 13.89ns = 69.4ns >= 40ns  [PASS, 裕量 +29.4ns]
 * 同时满足 tSD (25ns) 要求
 */
#define SRAM_DATA_SETUP_TIME 8U

/**
 * 总线转向时间 (Bus Turnaround Duration)
 * 用于读-写切换，SRAM 无需额外转向时间
 */
#define SRAM_BUS_TURNAROUND 0U

/* ------------------------------------------------------------------------- */
/*  函数声明                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief  初始化 SRAM (重新配置 FSMC 时序，替代 CubeMX 默认值)
 * @note   必须在 MX_FSMC_Init() 之后调用
 * @retval HAL_StatusTypeDef: HAL_OK 成功, HAL_ERROR 失败
 */
HAL_StatusTypeDef SRAM_Init(void);

/**
 * @brief  写入一个 16-bit 半字
 * @param  addr: 相对于 SRAM_BASE_ADDR 的字节偏移地址 (应为偶数)
 * @param  data: 待写入的 16-bit 数据
 */
void SRAM_WriteHalfWord(uint32_t addr, uint16_t data);

/**
 * @brief  读取一个 16-bit 半字
 * @param  addr: 相对于 SRAM_BASE_ADDR 的字节偏移地址 (应为偶数)
 * @retval 读取到的 16-bit 数据
 */
uint16_t SRAM_ReadHalfWord(uint32_t addr);

/**
 * @brief  写入一个 8-bit 字节
 * @param  addr: 相对于 SRAM_BASE_ADDR 的字节偏移地址
 * @param  data: 待写入的 8-bit 数据
 * @note   在 16-bit FSMC 模式下，FSMC 通过 NBL0/NBL1 自动处理字节选择:
 *         - 偶地址: NBL0 拉低，数据写入 D[7:0]
 *         - 奇地址: NBL1 拉低，数据写入 D[15:8]
 */
void SRAM_WriteByte(uint32_t addr, uint8_t data);

/**
 * @brief  读取一个 8-bit 字节
 * @param  addr: 相对于 SRAM_BASE_ADDR 的字节偏移地址
 * @retval 读取到的 8-bit 数据
 */
uint8_t SRAM_ReadByte(uint32_t addr);

/**
 * @brief  批量写入 16-bit 半字缓冲区
 * @param  addr: 起始地址 (相对于 SRAM_BASE_ADDR，应为偶数)
 * @param  buf:  源缓冲区指针
 * @param  len:  待写入的半字数量
 */
void SRAM_WriteBuffer(uint32_t addr, const uint16_t *buf, uint32_t len);

/**
 * @brief  批量读取 16-bit 半字缓冲区
 * @param  addr: 起始地址 (相对于 SRAM_BASE_ADDR，应为偶数)
 * @param  buf:  目标缓冲区指针
 * @param  len:  待读取的半字数量
 */
void SRAM_ReadBuffer(uint32_t addr, uint16_t *buf, uint32_t len);

/**
 * @brief  SRAM 全面自检
 * @param  startAddr: 测试起始地址 (相对于 SRAM_BASE_ADDR)
 * @param  size:      测试区域大小 (字节)
 * @retval HAL_StatusTypeDef: HAL_OK 测试通过, HAL_ERROR 测试失败
 *
 * 测试内容 (三阶段):
 *   1. 地址线测试 -- 写入地址特征值并校验，检测地址线短路/断路
 *   2. 数据线走马灯测试 (Walking 1s) -- 逐位验证，检测数据线短路/断路
 *   3. 棋盘格数据完整性测试 -- 交替写入 0xAAAA/0x5555 并校验
 */
HAL_StatusTypeDef SRAM_Test(uint32_t startAddr, uint32_t size);

/**
 * @brief  Dump FSMC Bank3 寄存器 (BCR3, BTR3) 到 UART，用于调试
 */
void SRAM_DumpRegisters(void);

/**
 * @brief  单点探测测试: 写一个值到 SRAM 地址 0 并读回
 * @retval HAL_StatusTypeDef: HAL_OK 读写一致, HAL_ERROR 不匹配
 * @note   如果此函数导致 MCU 卡死，说明 SRAM 硬件无响应
 */
HAL_StatusTypeDef SRAM_Probe(void);

#ifdef __cplusplus
}
#endif

#endif /* __IS62WV512_H */
