/**
 ******************************************************************************
 * @file    IS62WV512.c
 * @brief   IS62WV51216BLL-55TLI SRAM 驱动实现
 *          - 512K x 16 bit (8Mbit), 2.5V-3.6V, 55ns 访问速度
 *          - FSMC Bank3 (NE3), 16-bit 数据总线
 *          - NBL0(PE0)->LB, NBL1(PE1)->UB 标准字节使能接法
 *
 * 硬件连接:
 *   FSMC_A0~A18  -> SRAM A0~A18  (19根地址线, 寻址 512K x 16bit)
 *   FSMC_D0~D15  -> SRAM I/O0~I/O15
 *   FSMC_NE3     -> SRAM CS1 (低电平有效)
 *   FSMC_NOE     -> SRAM OE  (低电平有效)
 *   FSMC_NWE     -> SRAM WE  (低电平有效)
 *   FSMC_NBL0    -> SRAM LB  (低电平有效, 使能 D[7:0])
 *   FSMC_NBL1    -> SRAM UB  (低电平有效, 使能 D[15:8])
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "IS62WV512.h"
#include "fsmc.h"
#include <stdio.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/

/**
 * @brief  SRAM 绝对地址指针转换宏
 *         将相对偏移转换为 FSMC 内存映射区的绝对指针
 */
#define SRAM_ADDR16(offset) ((volatile uint16_t *)(SRAM_BASE_ADDR + (offset)))
#define SRAM_ADDR8(offset) ((volatile uint8_t *)(SRAM_BASE_ADDR + (offset)))

/* Private variables ---------------------------------------------------------*/

/** SRAM 测试走马灯步数 (每个阶段测试的采样点数) */
#define SRAM_TEST_ADDR_POINTS 32U
#define SRAM_TEST_WALKING_POINTS 256U

/* ------------------------------------------------------------------------- */
/*  FSMC 时序初始化                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief  初始化 SRAM 时序配置
 *
 * CubeMX 生成的默认时序 (ADDSET=1, DATAST=4) 地址建立时间不足:
 *   默认 ADDSET=1: (1+1) x 13.89ns = 27.8ns < tACS(55ns)  [FAIL]
 *
 * 本函数使用优化后的时序参数，确保满足 IS62WV51216BLL-55TLI 规格书要求:
 *   ADDSET=4: (4+1) x 13.89ns = 69.4ns >= tACS(55ns)  [PASS]
 *   DATAST=4: (4+1) x 13.89ns = 69.4ns >= tPWE(40ns)  [PASS]
 *   BUSTURN=0: 最小总线转向时间
 *   读周期: (4+4+2) x 13.89ns = 138.9ns >= tRC(55ns)  [PASS]
 *   写周期: (4+4+0+3) x 13.89ns = 152.8ns >= tWC(55ns) [PASS]
 */
HAL_StatusTypeDef SRAM_Init(void) {
  FSMC_NORSRAM_TimingTypeDef sTiming = {0};

  /* 重新配置 hsram2 (Bank3, NE3) 的时序参数 */
  hsram2.Init.NSBank = FSMC_NORSRAM_BANK3;
  hsram2.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
  hsram2.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
  hsram2.Init.MemoryDataWidth =
      FSMC_NORSRAM_MEM_BUS_WIDTH_16; /* 16-bit 数据总线 */
  hsram2.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
  hsram2.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram2.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
  hsram2.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
  hsram2.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
  hsram2.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
  hsram2.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
  hsram2.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram2.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;

  /*
   * 时序参数设置 (HCLK = 72MHz, tHCLK = 13.89ns)
   * ---------------------------------------------------------------
   * AddressSetupTime: 地址建立时间
   *   实际: (4+1) x 13.89ns = 69.4ns
   *   要求: >= tACS = 55ns  -> 裕量 +14.4ns
   *
   * AddressHoldTime: 地址保持时间 (16-bit 非复用模式下无实际作用，填最小值)
   *
   * DataSetupTime: 数据建立时间
   *   实际: (4+1) x 13.89ns = 69.4ns
   *   要求: >= tPWE = 40ns  -> 裕量 +29.4ns
   *   同时满足 tSD (Data Setup) = 25ns
   *
   * BusTurnAroundDuration: 总线转向时间 (读->写切换)
   *   实际: (0+1) x 13.89ns = 13.89ns
   *   SRAM 无需额外转向时间，设为 0
   *
   * AccessMode: FSMC_ACCESS_MODE_A (标准异步 SRAM 访问模式)
   */
  sTiming.AddressSetupTime = SRAM_ADDR_SETUP_TIME; /* 4 */
  sTiming.AddressHoldTime = 1U; /* 最小值，非复用模式无实际作用 */
  sTiming.DataSetupTime = SRAM_DATA_SETUP_TIME;        /* 4 */
  sTiming.BusTurnAroundDuration = SRAM_BUS_TURNAROUND; /* 0 */
  sTiming.CLKDivision = 16U; /* 同步模式才有效，异步模式无作用 */
  sTiming.DataLatency = 17U; /* 同步模式才有效，异步模式无作用 */
  sTiming.AccessMode = FSMC_ACCESS_MODE_A;

  if (HAL_SRAM_Init(&hsram2, &sTiming, NULL) != HAL_OK) {
    printf("[SRAM] ERROR: HAL_SRAM_Init failed!\r\n");
    return HAL_ERROR;
  }

  printf("[SRAM] Init OK: Bank3 @ 0x%08lX, ADDSET=%lu, DATAST=%lu\r\n",
         (unsigned long)SRAM_BASE_ADDR, (unsigned long)SRAM_ADDR_SETUP_TIME,
         (unsigned long)SRAM_DATA_SETUP_TIME);

  return HAL_OK;
}

/* ------------------------------------------------------------------------- */
/*  读写操作函数                                                            */
/* ------------------------------------------------------------------------- */

/**
 * @brief  写入一个 16-bit 半字
 * @param  addr: 字节偏移地址 (应为偶数，否则将截断为偶数地址)
 * @param  data: 待写入的 16-bit 数据
 *
 * FSMC 16-bit 模式下，半字写入直接映射到 D[15:0] 数据线，
 * NBL0 和 NBL1 同时拉低，使能全部 16 位数据线。
 */
void SRAM_WriteHalfWord(uint32_t addr, uint16_t data) {
  /* 强制对齐到偶地址 */
  addr &= 0xFFFFFFFEU;
  *SRAM_ADDR16(addr) = data;
}

/**
 * @brief  读取一个 16-bit 半字
 * @param  addr: 字节偏移地址 (应为偶数)
 * @retval 读取到的 16-bit 数据
 */
uint16_t SRAM_ReadHalfWord(uint32_t addr) {
  addr &= 0xFFFFFFFEU;
  return *SRAM_ADDR16(addr);
}

/**
 * @brief  写入一个 8-bit 字节
 * @param  addr: 字节偏移地址
 * @param  data: 待写入的 8-bit 数据
 *
 * 在 FSMC 16-bit 总线 + 标准 NBL 接法下，FSMC 硬件自动处理字节选择:
 *
 * 当访问偶地址 (A0=0) 时:
 *   FSMC 拉低 NBL0 (LB)，NBL1 (UB) 保持高
 *   只有 D[7:0] 被写入 SRAM，高 8 位被屏蔽
 *   数据放在 FSMC_D0~D7
 *
 * 当访问奇地址 (A0=1) 时:
 *   FSMC 拉低 NBL1 (UB)，NBL0 (LB) 保持高
 *   只有 D[15:8] 被写入 SRAM，低 8 位被屏蔽
 *   FSMC 内部将数据移位到 D[15:8]
 *
 * 因此使用 uint8_t 指针直接访问即可，FSMC 硬件完全透明处理。
 */
void SRAM_WriteByte(uint32_t addr, uint8_t data) { *SRAM_ADDR8(addr) = data; }

/**
 * @brief  读取一个 8-bit 字节
 * @param  addr: 字节偏移地址
 * @retval 读取到的 8-bit 数据
 *
 * 读取原理同写入，FSMC 根据地址 A0 自动选择 NBL0/NBL1:
 *   偶地址: 从 D[7:0] 读取
 *   奇地址: 从 D[15:8] 读取，FSMC 内部移位返回低 8 位
 */
uint8_t SRAM_ReadByte(uint32_t addr) { return *SRAM_ADDR8(addr); }

/**
 * @brief  批量写入 16-bit 半字缓冲区
 * @param  addr: 起始字节偏移 (应为偶数)
 * @param  buf:  源缓冲区
 * @param  len:  半字数量
 */
void SRAM_WriteBuffer(uint32_t addr, const uint16_t *buf, uint32_t len) {
  volatile uint16_t *dst = SRAM_ADDR16(addr & 0xFFFFFFFEU);
  uint32_t i;

  for (i = 0U; i < len; i++) {
    dst[i] = buf[i];
  }
}

/**
 * @brief  批量读取 16-bit 半字缓冲区
 * @param  addr: 起始字节偏移 (应为偶数)
 * @param  buf:  目标缓冲区
 * @param  len:  半字数量
 */
void SRAM_ReadBuffer(uint32_t addr, uint16_t *buf, uint32_t len) {
  volatile uint16_t *src = SRAM_ADDR16(addr & 0xFFFFFFFEU);
  uint32_t i;

  for (i = 0U; i < len; i++) {
    buf[i] = src[i];
  }
}

/* ------------------------------------------------------------------------- */
/*  调试辅助函数                                                            */
/* ------------------------------------------------------------------------- */

/**
 * @brief  Dump FSMC Bank3 寄存器到 UART
 *
 * BCR3 (Bank Control Register):
 *   bit 0:   MBKEN  - Memory bank enable
 *   bit 1:   MUXEN  - Address/data multiplexing
 *   bit 3:2: MTYP   - Memory type (00=SRAM)
 *   bit 5:4: MWID   - Memory data bus width (01=16bit)
 *   bit 8:   WREN   - Write enable
 *   bit 14:  EXTMOD - Extended mode enable
 *
 * BTR3 (Bank Timing Register):
 *   bit 3:0:  ADDSET  - Address setup time
 *   bit 7:4:  ADDHLD  - Address hold time
 *   bit 15:8: DATAST  - Data setup time
 *   bit 19:16: BUSTURN - Bus turnaround
 *   bit 23:20: CLKDIV  - Clock divide ratio
 *   bit 27:24: DATLAT  - Data latency
 *   bit 29:28: ACCMOD  - Access mode
 */
void SRAM_DumpRegisters(void) {
  /* FSMC Bank3 寄存器: FSMC_Bank1->BTCR[4] = BCR3, BTCR[5] = BTR3 */
  uint32_t bcr_val = FSMC_Bank1->BTCR[4];
  uint32_t btr_val = FSMC_Bank1->BTCR[5];

  printf("\r\n[FSMC Register Dump - Bank3]\r\n");
  printf("  BCR3 = 0x%08lX\r\n", (unsigned long)bcr_val);
  printf("    MBKEN  = %lu  (Bank enable)\r\n",
         (unsigned long)(bcr_val & 0x1U));
  printf("    MUXEN  = %lu  (Addr/Data mux)\r\n",
         (unsigned long)((bcr_val >> 1) & 0x1U));
  printf("    MTYP   = %lu  (Memory type: 0=SRAM)\r\n",
         (unsigned long)((bcr_val >> 2) & 0x3U));
  printf("    MWID   = %lu  (Bus width: 0=8bit, 1=16bit)\r\n",
         (unsigned long)((bcr_val >> 4) & 0x3U));
  printf("    WREN   = %lu  (Write enable)\r\n",
         (unsigned long)((bcr_val >> 12) & 0x1U));
  printf("    EXTMOD = %lu  (Extended mode)\r\n",
         (unsigned long)((bcr_val >> 14) & 0x1U));

  printf("  BTR3 = 0x%08lX\r\n", (unsigned long)btr_val);
  printf("    ADDSET  = %lu  (x tHCLK = %lu ns)\r\n",
         (unsigned long)(btr_val & 0xFU),
         (unsigned long)((btr_val & 0xFU) + 1U) * 14U); /* ~13.89ns rounded */
  printf("    ADDHLD  = %lu\r\n", (unsigned long)((btr_val >> 4) & 0xFU));
  printf("    DATAST  = %lu  (x tHCLK = %lu ns)\r\n",
         (unsigned long)((btr_val >> 8) & 0xFFU),
         (unsigned long)(((btr_val >> 8) & 0xFFU) + 1U) * 14U);
  printf("    BUSTURN = %lu\r\n", (unsigned long)((btr_val >> 16) & 0xFU));
  printf("    CLKDIV  = %lu\r\n", (unsigned long)((btr_val >> 20) & 0xFU));
  printf("    DATLAT  = %lu\r\n", (unsigned long)((btr_val >> 24) & 0xFU));
  printf("    ACCMOD  = %lu  (0=Mode A)\r\n",
         (unsigned long)((btr_val >> 28) & 0x3U));
}

/**
 * @brief  单点探测测试
 *
 * 在 SRAM 地址 0 处写入 0xA55A，然后读回校验。
 * 这是最简单的连通性测试:
 *   - 如果写入后读回一致: SRAM 基本工作正常
 *   - 如果读回全 0xFFFF 或全 0x0000: SRAM 未响应 (检查硬件)
 *   - 如果读回值不一致: 数据线可能有短路
 *   - 如果函数卡死: FSMC 总线挂死 (硬件严重问题)
 */
HAL_StatusTypeDef SRAM_Probe(void) {
  uint16_t testPattern = 0xA55AU;
  uint16_t readBack;

  printf("\r\n[SRAM Probe] Single-point connectivity test...\r\n");

  /* 先读一次当前值 (查看上电默认状态) */
  readBack = *SRAM_ADDR16(0U);
  printf("  Read before write @ 0x%08lX: 0x%04X\r\n",
         (unsigned long)SRAM_BASE_ADDR, readBack);

  /* 写入测试图案 */
  *SRAM_ADDR16(0U) = testPattern;

  /* 插入几个 NOP 确保写入完成 (虽然 FSMC 写入是同步的) */
  __NOP();
  __NOP();
  __NOP();
  __NOP();

  /* 读回校验 */
  readBack = *SRAM_ADDR16(0U);
  printf("  Write 0x%04X, Read back: 0x%04X", testPattern, readBack);

  if (readBack == testPattern) {
    printf("  -> MATCH! SRAM is alive.\r\n");
    return HAL_OK;
  } else if (readBack == 0xFFFFU || readBack == 0x0000U) {
    printf("  -> FAIL! Bus floating (0x%04X). Check: CS, power, NBL pins.\r\n",
           readBack);
  } else {
    printf("  -> MISMATCH! Possible data line issue.\r\n");
  }

  return HAL_ERROR;
}

/* ------------------------------------------------------------------------- */
/*  SRAM 自检函数                                                           */
/* ------------------------------------------------------------------------- */

/**
 * @brief  SRAM 全面自检 (三阶段)
 *
 * 阶段 1: 地址线测试
 *   在 SRAM_TEST_ADDR_POINTS 个分散地址点写入地址特征值 (addr & 0xFFFF)，
 *   然后逐个读回校验。可检测地址线短路、断路、相邻地址互相干扰。
 *
 * 阶段 2: 数据线走马灯测试 (Walking 1s)
 *   在基准地址依次写入 1<<0, 1<<1, ..., 1<<15 并立即读回，
 *   验证每根数据线独立工作能力，检测数据线短路/断路。
 *   同时在多个偏移地址重复此测试，覆盖更多物理位置。
 *
 * 阶段 3: 棋盘格数据完整性测试
 *   交替写入 0xAAAA 和 0x5555 图案覆盖整个测试区域，
 *   然后读回校验。可检测相邻存储单元干扰、数据保持等问题。
 */
HAL_StatusTypeDef SRAM_Test(uint32_t startAddr, uint32_t size) {
  uint32_t errors = 0U;
  uint32_t i;
  uint16_t readVal;
  uint16_t expected;

  /* 确保测试区域不超出 SRAM 容量 */
  if ((startAddr + size) > SRAM_SIZE_BYTES) {
    size = SRAM_SIZE_BYTES - startAddr;
  }
  /* 确保至少按半字对齐 */
  size &= 0xFFFFFFFEU;

  printf("\r\n===== SRAM Test Begin =====\r\n");
  printf("Base: 0x%08lX, Start: 0x%08lX, Size: %lu bytes\r\n",
         (unsigned long)SRAM_BASE_ADDR,
         (unsigned long)(SRAM_BASE_ADDR + startAddr), (unsigned long)size);

  /* ===================================================================== */
  /* 阶段 1: 地址线测试                                                     */
  /* ===================================================================== */
  printf("\r\n[Phase 1] Address Line Test...\r\n");
  {
    /*
     * 在测试区域内均匀选取 SRAM_TEST_ADDR_POINTS 个地址点，
     * 写入地址低 16 位作为特征值，然后读回校验。
     * 地址间隔 = size / SRAM_TEST_ADDR_POINTS，确保覆盖全区域。
     */
    uint32_t step = (size / SRAM_TEST_ADDR_POINTS) & 0xFFFFFFFEU;
    if (step < 2U) {
      step = 2U;
    }

    /* 写入阶段 */
    for (i = 0U; i < SRAM_TEST_ADDR_POINTS; i++) {
      uint32_t offset = startAddr + (i * step);
      if (offset >= (startAddr + size)) {
        break;
      }
      uint16_t pattern = (uint16_t)(offset & 0xFFFFU);
      SRAM_WriteHalfWord(offset, pattern);
    }

    /* 校验阶段 */
    for (i = 0U; i < SRAM_TEST_ADDR_POINTS; i++) {
      uint32_t offset = startAddr + (i * step);
      if (offset >= (startAddr + size)) {
        break;
      }
      expected = (uint16_t)(offset & 0xFFFFU);
      readVal = SRAM_ReadHalfWord(offset);
      if (readVal != expected) {
        if (errors < 8U) {
          printf("  FAIL @ addr 0x%08lX: wrote 0x%04X, read 0x%04X\r\n",
                 (unsigned long)(SRAM_BASE_ADDR + offset), expected, readVal);
        }
        errors++;
      }
    }
    printf("  Address test: %lu errors\r\n", (unsigned long)errors);
    if (errors > 0U) {
      printf("===== SRAM Test FAILED (Phase 1) =====\r\n");
      return HAL_ERROR;
    }
  }

  /* ===================================================================== */
  /* 阶段 2: 数据线走马灯测试 (Walking 1s)                                  */
  /* ===================================================================== */
  printf("\r\n[Phase 2] Data Line Walking-1s Test...\r\n");
  {
    uint32_t phase2_errors = 0U;
    uint32_t stride = (size / SRAM_TEST_WALKING_POINTS) & 0xFFFFFFFEU;
    if (stride < 2U) {
      stride = 2U;
    }

    /*
     * 在多个偏移地址处执行走马灯测试:
     * 对每根数据线 (bit0~bit15)，依次单独置 1 并读回，
     * 验证该位独立工作能力。
     */
    for (i = 0U; i < SRAM_TEST_WALKING_POINTS; i++) {
      uint32_t offset = startAddr + (i * stride);
      if (offset >= (startAddr + size)) {
        break;
      }

      uint8_t bit;
      for (bit = 0U; bit < 16U; bit++) {
        uint16_t pattern = (uint16_t)(1U << bit);
        SRAM_WriteHalfWord(offset, pattern);
        readVal = SRAM_ReadHalfWord(offset);
        if (readVal != pattern) {
          if (phase2_errors < 8U) {
            printf("  FAIL @ addr 0x%08lX bit%u: wrote 0x%04X, read 0x%04X\r\n",
                   (unsigned long)(SRAM_BASE_ADDR + offset), (unsigned int)bit,
                   pattern, readVal);
          }
          phase2_errors++;
        }
      }
    }
    errors += phase2_errors;
    printf("  Walking-1s test: %lu errors\r\n", (unsigned long)phase2_errors);
    if (phase2_errors > 0U) {
      printf("===== SRAM Test FAILED (Phase 2) =====\r\n");
      return HAL_ERROR;
    }
  }

  /* ===================================================================== */
  /* 阶段 3: 棋盘格数据完整性测试                                           */
  /* ===================================================================== */
  printf("\r\n[Phase 3] Checkerboard Pattern Test...\r\n");
  {
    uint32_t phase3_errors = 0U;
    uint32_t totalHalfWords = size / 2U;

    /* 写入: 交替 0xAAAA / 0x5555 */
    for (i = 0U; i < totalHalfWords; i++) {
      uint32_t offset = startAddr + (i * 2U);
      uint16_t pattern = ((i & 1U) == 0U) ? 0xAAAAU : 0x5555U;
      SRAM_WriteHalfWord(offset, pattern);
    }

    /* 校验: 读回并比对 */
    for (i = 0U; i < totalHalfWords; i++) {
      uint32_t offset = startAddr + (i * 2U);
      expected = ((i & 1U) == 0U) ? 0xAAAAU : 0x5555U;
      readVal = SRAM_ReadHalfWord(offset);
      if (readVal != expected) {
        if (phase3_errors < 8U) {
          printf("  FAIL @ addr 0x%08lX: wrote 0x%04X, read 0x%04X\r\n",
                 (unsigned long)(SRAM_BASE_ADDR + offset), expected, readVal);
        }
        phase3_errors++;
      }
    }
    errors += phase3_errors;
    printf("  Checkerboard test: %lu errors (%lu halfwords tested)\r\n",
           (unsigned long)phase3_errors, (unsigned long)totalHalfWords);
  }

  /* ===================================================================== */
  /* 汇总结果                                                              */
  /* ===================================================================== */
  printf("\r\n===== SRAM Test %s (total errors: %lu) =====\r\n",
         (errors == 0U) ? "PASSED" : "FAILED", (unsigned long)errors);

  return (errors == 0U) ? HAL_OK : HAL_ERROR;
}
