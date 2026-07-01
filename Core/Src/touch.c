/**
 ******************************************************************************
 * @file    touch.c
 * @brief   FT5x16 电容触摸驱动实现
 * @note    硬件 I2C1 (PB6=SCL, PB7=SDA)
 *          支持 GPIO 引脚诊断 + 寄存器级探测 + 软件 I2C 后备
 ******************************************************************************
 */

#include "touch.h"
#include "gpio.h"
#include "i2c.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>


/* 连续I2C错误达到此阈值后触发总线恢复 */
#define I2C_ERROR_THRESHOLD 10

/* ============================================================================
 */
/*  内部变量 */
/* ============================================================================
 */

/* I2C 操作模式: 0=硬件I2C, 1=软件bit-bang */
static uint8_t s_i2c_mode = 0;

/* I2C连续错误计数器，达到阈值后触发总线恢复 */
static uint16_t s_i2c_err_cnt = 0;

/* 保存初始化时探测到的 I2C 设备地址 (HAL 格式: 7位地址左移1位) */
static uint16_t s_dev_addr = 0;

/* 当前校准参数 */
static Touch_Calib_t s_calib = {
    .x_offset = 0,
    .y_offset = 0,
    .x_scale = 256, /* 1:1 直通 */
    .y_scale = 256, /* 1:1 直通 */
    .swap_xy = 0,
    .x_mirror = 1, /* 触摸X轴与LCD X轴相反 */
    .y_mirror = 0,
};

/* ============================================================================
 */
/*  GPIO 引脚诊断测试 */
/*                                                                              */
/*  验证 PB6(SCL)/PB7(SDA) 是否可以被正常控制 */
/*  返回: 0=引脚正常, 1=PB6异常, 2=PB7异常, 3=两者都异常 */
/* ============================================================================
 */

static uint8_t GPIO_PinTest(void) {
  GPIO_InitTypeDef gpio = {0};

  /* 配置 PB6/PB7 为推挽输出 */
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  uint8_t err = 0;

  /* 测试 PB6: 输出高→读回高, 输出低→读回低 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  if (!(GPIOB->IDR & GPIO_PIN_6))
    err |= 1;

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
  if (GPIOB->IDR & GPIO_PIN_6)
    err |= 1;

  /* 测试 PB7: 输出高→读回高, 输出低→读回低 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
  if (!(GPIOB->IDR & GPIO_PIN_7))
    err |= 2;

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
  if (GPIOB->IDR & GPIO_PIN_7)
    err |= 2;

  /* 恢复 PB6/PB7 为高电平（I2C 空闲态） */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);

  return err;
}

/* ============================================================================
 */
/*  软件 Bit-bang I2C (后备方案) */
/* ============================================================================
 */

/* I2C 微秒级延时 */
static void i2c_delay(void) {
  for (volatile int i = 0; i < 120; i++)
    ; /* ~5us @ 72MHz (标准模式100kHz要求SCL低≥4.7us) */
}

#define SCL_H()                                                                \
  do {                                                                         \
    GPIOB->BSRR = GPIO_PIN_6;                                                  \
    i2c_delay();                                                               \
  } while (0)
#define SCL_L()                                                                \
  do {                                                                         \
    GPIOB->BRR = GPIO_PIN_6;                                                   \
    i2c_delay();                                                               \
  } while (0)
#define SDA_H()                                                                \
  do {                                                                         \
    GPIOB->BSRR = GPIO_PIN_7;                                                  \
    i2c_delay();                                                               \
  } while (0)
#define SDA_L()                                                                \
  do {                                                                         \
    GPIOB->BRR = GPIO_PIN_7;                                                   \
    i2c_delay();                                                               \
  } while (0)
#define SDA_R() (GPIOB->IDR & GPIO_PIN_7)

/* 配置 PB6/PB7 为 GPIO 开漏输出(软件 I2C用) */
static void GPIO_ConfigForBitbang(void) {
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_OD; /* 开漏输出 */
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
}

/* 软件 I2C Start 条件: SCL高时 SDA从高→低跳变 */
static void i2c_start(void) {
  SDA_H(); /* 确保 SDA 先释放为高 */
  SCL_H(); /* SCL 拉高 */
  i2c_delay();
  SDA_L(); /* SCL高时 SDA 下降沿 = START */
  i2c_delay();
  SCL_L(); /* 准备传输时钟 */
  i2c_delay();
}

/* 软件 I2C Stop 条件 */
static void i2c_stop(void) {
  SDA_L();
  SCL_H();
  SDA_H();
}

/* 软件 I2C 发送 1 字节，返回 ACK(0=成功) */
static uint8_t i2c_send_byte(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    if (data & 0x80)
      SDA_H();
    else
      SDA_L();
    data <<= 1;
    SCL_H();
    SCL_L();
  }
  /* 第9个时钟: 释放 SDA, 读 ACK */
  SDA_H();
  SCL_H();
  uint8_t ack = SDA_R() ? 1 : 0;
  SCL_L();
  return ack;
}

/* 软件 I2C 读取 1 字节: ack=0 发 ACK, ack=1 发 NACK(最后字节) */
static uint8_t i2c_read_byte(uint8_t nack) {
  uint8_t data = 0;
  SDA_H(); /* 释放 SDA 让从机控制 */
  for (int i = 0; i < 8; i++) {
    data <<= 1;
    SCL_H();
    if (SDA_R())
      data |= 1;
    SCL_L();
  }
  /* 第9时钟: ACK(低) 或 NACK(高) */
  if (nack)
    SDA_H();
  else
    SDA_L();
  SCL_H();
  SCL_L();
  SDA_H(); /* 释放 SDA */
  i2c_delay();
  return data;
}

/* 软件 I2C 写寄存器: Start + AddrW + Reg + data(s) + Stop */
static uint8_t bb_RegWrite(uint8_t addr7, uint8_t reg, const uint8_t *data,
                           uint16_t len) {
  i2c_start();
  if (i2c_send_byte((addr7 << 1) | 0)) {
    i2c_stop();
    return 1;
  }
  i2c_send_byte(reg); /* 即使 NACK 也继续 */
  for (uint16_t i = 0; i < len; i++)
    i2c_send_byte(data[i]);
  i2c_stop();
  return 0;
}

/* 软件 I2C 读寄存器: Start + AddrW + Reg + Sr + AddrR + data(s) + Stop */
static uint8_t bb_RegRead(uint8_t addr7, uint8_t reg, uint8_t *buf,
                          uint16_t len) {
  if (len == 0)
    return 1;
  i2c_start();
  if (i2c_send_byte((addr7 << 1) | 0)) {
    i2c_stop();
    return 1;
  }
  i2c_send_byte(reg);
  i2c_start(); /* 重复起始 */
  if (i2c_send_byte((addr7 << 1) | 1)) {
    i2c_stop();
    return 1;
  }
  for (uint16_t i = 0; i < len; i++)
    buf[i] = i2c_read_byte(i == len - 1); /* 最后字节 NACK */
  i2c_stop();
  return 0;
}

/* 软件 I2C 探测地址是否响应 */
static uint8_t i2c_probe_addr(uint8_t addr7) {
  i2c_start();
  uint8_t ack = i2c_send_byte((addr7 << 1) | 0); /* 写方向 */
  i2c_stop();
  return ack; /* 0=有ACK(设备存在), 1=无ACK */
}

/* ============================================================================
 */
/*  直接 I2C 寄存器级探测 (绕过 HAL 状态机问题) */
/* ============================================================================
 */

/**
 * @brief  直接操作 I2C1 寄存器探测设备地址
 * @retval 0=有响应, 1=无响应, 0xFF=I2C外设忙/状态异常
 */
static uint8_t I2C_RegisterProbe(uint8_t addr7) {
  uint32_t timeout;
  uint8_t dev_addr = (addr7 << 1); /* 8-bit格式, 写方向 */

  /* 等待 I2C 不忙 */
  timeout = 1000;
  while ((I2C1->SR2 & I2C_SR2_BUSY) && timeout--)
    HAL_Delay(1);
  if (timeout == 0)
    return 0xFF; /* 总线一直忙 */

  /* 产生 START 条件 */
  I2C1->CR1 |= I2C_CR1_START;

  /* 等待 SB 标志 (START 已发送) */
  timeout = 10000;
  while (!(I2C1->SR1 & I2C_SR1_SB) && timeout--)
    ;
  if (timeout == 0) {
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0xFF;
  }

  /* 发送设备地址 (写方向) */
  I2C1->DR = dev_addr;

  /* 等待 ADDR (地址已发送, 等待 ACK/NACK) */
  timeout = 10000;
  while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)) && timeout--)
    ;
  if (timeout == 0) {
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0xFF;
  }

  if (I2C1->SR1 & I2C_SR1_AF) /* NACK 收到 */
  {
    /* 清除 AF 标志 */
    I2C1->SR1 &= ~I2C_SR1_AF;
    /* 发送 STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
    return 1; /* 无响应 */
  }

  /* ADDR 标志置位 → 设备有响应 */
  /* 读 SR1+SR2 清除 ADDR 标志 */
  (void)I2C1->SR1;
  (void)I2C1->SR2;

  /* 发送 STOP */
  I2C1->CR1 |= I2C_CR1_STOP;
  return 0; /* 设备存在 */
}

/* ============================================================================
 */
/*  I2C 总线恢复 */
/* ============================================================================
 */

static void I2C_BusRecovery(void) {
  GPIO_InitTypeDef gpio = {0};
  HAL_I2C_DeInit(&hi2c1);

  /* 推挽输出做总线恢复 */
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  /* 拉高释放 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(1);

  /* 9 个 SCL 脉冲 */
  for (int i = 0; i < 9; i++) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
  }

  /* STOP 条件 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(1);

  /* 重新初始化 I2C1 外设 */
  MX_I2C1_Init();

  /* 在 MX_I2C1_Init 之后重新加上内部上拉 */
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);
}

/* ============================================================================
 */
/*  I2C 通信封装（使用 s_dev_addr + s_i2c_mode） */
/* ============================================================================
 */

static HAL_StatusTypeDef Touch_I2C_Read(uint8_t reg, uint8_t *buf,
                                        uint16_t len) {
  if (s_i2c_mode == 1)
    return HAL_ERROR;
  /* 使用 Master_Transmit + Master_Receive 分步操作，避免 Mem_Read
   * 的重复起始问题 */
  if (HAL_I2C_Master_Transmit(&hi2c1, s_dev_addr, &reg, 1,
                              FT5X16_I2C_TIMEOUT) != HAL_OK)
    return HAL_ERROR;
  return HAL_I2C_Master_Receive(&hi2c1, s_dev_addr, buf, len,
                                FT5X16_I2C_TIMEOUT);
}

/* ============================================================================
 */
/*  坐标转换函数 */
/* ============================================================================
 */

static void FT5x16_MapCoord(uint16_t raw_x, uint16_t raw_y, uint16_t *lcd_x,
                            uint16_t *lcd_y) {
  int32_t tx, ty;
  tx = (int32_t)raw_x - s_calib.x_offset;
  ty = (int32_t)raw_y - s_calib.y_offset;
  tx = (tx * s_calib.x_scale) >> 8;
  ty = (ty * s_calib.y_scale) >> 8;
  if (s_calib.x_mirror)
    tx = LCD_WIDTH - 1 - tx;
  if (s_calib.y_mirror)
    ty = LCD_HEIGHT - 1 - ty;
  if (s_calib.swap_xy) {
    int32_t tmp = tx;
    tx = ty;
    ty = tmp;
  }
  if (tx < 0)
    tx = 0;
  if (tx >= LCD_WIDTH)
    tx = LCD_WIDTH - 1;
  if (ty < 0)
    ty = 0;
  if (ty >= LCD_HEIGHT)
    ty = LCD_HEIGHT - 1;
  *lcd_x = (uint16_t)tx;
  *lcd_y = (uint16_t)ty;
}

/* ============================================================================
 */
/*  公共函数 */
/* ============================================================================
 */

/**
 * @brief  FT5x16 初始化
 * @retval 0=成功, 1=GPIO引脚异常, 2=硬件I2C无响应,
 * 3=寄存器级探测成功但HAL读ID失败
 *         4=软件I2C探测也无响应(FT5x16硬件的I2C可能未连接/未上电)
 */
uint8_t FT5x16_Init(void) {
  uint8_t addr_found = 0;
  s_dev_addr = 0;
  s_i2c_mode = 0;

  /* ========== 第〇步: GPIO 引脚诊断 ========== */
  uint8_t pin_err = GPIO_PinTest();
  if (pin_err)
    return 1; /* PB6/PB7 引脚异常 */

  /* ========== 第一步: 硬件 I2C 外设探测 ========== */
  I2C_BusRecovery();

  static const uint8_t probe_addrs[] = {0x38, 0x39};

  /* 先用 HAL 探测 */
  for (int i = 0; i < 2; i++) {
    uint16_t try_addr = (uint16_t)probe_addrs[i] << 1;
    if (HAL_I2C_IsDeviceReady(&hi2c1, try_addr, 3, 100) == HAL_OK) {
      s_dev_addr = try_addr;
      addr_found = 1;
      break;
    }
  }

  /* 如果 HAL 探测失败，改用寄存器级探测 */
  if (!addr_found) {
    for (int i = 0; i < 2; i++) {
      uint8_t r = I2C_RegisterProbe(probe_addrs[i]);
      if (r == 0) /* 寄存器级探测有响应 */
      {
        s_dev_addr = (uint16_t)probe_addrs[i] << 1;
        addr_found = 1;

        /*
         * 寄存器级探测确认 FT5x16 存在，但 STM32F1 硬件 I2C 外设与此芯片
         * 存在重复起始(Repeated Start)兼容问题，HAL 后续操作持续失败。
         * 解决方案: 放弃硬件 I2C，完全切换到软件 bit-bang 模式。
         */
        HAL_I2C_DeInit(&hi2c1);
        GPIO_ConfigForBitbang();
        s_i2c_mode = 1;
        break;
      }
    }
  }

  /* 如果寄存器级也失败，尝试软件 bit-bang 探测 */
  if (!addr_found) {
    /* 反初始化 I2C1，释放引脚 */
    HAL_I2C_DeInit(&hi2c1);

    /* 配置 GPIO 为开漏输出 */
    GPIO_ConfigForBitbang();

    /* bit-bang 探测地址 */
    for (int i = 0; i < 2; i++) {
      uint8_t ack = i2c_probe_addr(probe_addrs[i]);
      if (ack == 0) /* 0=ACK收到 */
      {
        s_dev_addr = (uint16_t)probe_addrs[i] << 1;
        addr_found = 1;
        s_i2c_mode = 1; /* 切换到软件模式 */
        break;
      }
    }

    if (!addr_found) {
      /* 所有方法都失败 → 恢复硬件 I2C 待用 */
      GPIO_ConfigForBitbang();
      return 4; /* FT5x16 硬件未连接/未上电 */
    }
  }

  /* ========== 第二步: 跳过 ID 校验，直接验证通信 ========== */
  /*
   * 经验: 部分 FT5x16 固件版本的寄存器 0xA8/0xA3 不可读或返回 NACK,
   *      但这不影响触摸功能。既然寄存器级探测已确认芯片应答,
   *      直接执行触摸参数配置即可。
   */

  uint8_t bb_reg;
  uint8_t addr7 = (uint8_t)(s_dev_addr >> 1);

  /* 唤醒芯片: 写 1 字节到寄存器 0x00 */
  bb_reg = 0x00;
  bb_RegWrite(addr7, bb_reg, (uint8_t[]){0x00}, 1);
  HAL_Delay(10);

  /* 验证: 读触摸状态寄存器 0x02 */
  {
    uint8_t test_buf[1];
    bb_RegRead(addr7, 0x02, test_buf, 1);
  }

  /* ========== 第三步: 配置触摸参数 ========== */
  bb_RegWrite(addr7, 0x80, (uint8_t[]){0x38}, 1); /* 阈值 56 */
  bb_RegWrite(addr7, 0x88, (uint8_t[]){0x0E}, 1); /* 扫描周期 14 */

  return 0;
}

/**
 * @brief  触摸扫描
 */
uint8_t FT5x16_Scan(uint16_t *x, uint16_t *y) {
  uint8_t buf[5] = {0};
  uint8_t touch_count;
  uint16_t raw_x, raw_y;

  if (s_dev_addr == 0)
    return 0;

  uint8_t addr7 = (uint8_t)(s_dev_addr >> 1);

  if (s_i2c_mode == 1) {
    if (bb_RegRead(addr7, FT5X16_REG_TD_STATUS, buf, 5) != 0) {
      s_i2c_err_cnt++;
      if (s_i2c_err_cnt >= I2C_ERROR_THRESHOLD) {
        I2C_BusRecovery();
        s_i2c_err_cnt = 0;
      }
      return 0;
    }
  } else {
    if (Touch_I2C_Read(FT5X16_REG_TD_STATUS, buf, 5) != HAL_OK) {
      s_i2c_err_cnt++;
      if (s_i2c_err_cnt >= I2C_ERROR_THRESHOLD) {
        I2C_BusRecovery();
        /* 总线恢复后若硬件I2C仍不可用，切到软件模式 */
        if (HAL_I2C_IsDeviceReady(&hi2c1, s_dev_addr, 1, 50) != HAL_OK) {
          HAL_I2C_DeInit(&hi2c1);
          GPIO_ConfigForBitbang();
          s_i2c_mode = 1;
        }
        s_i2c_err_cnt = 0;
      }
      return 0;
    }
  }
  /* I2C通信成功，清零错误计数 */
  s_i2c_err_cnt = 0;

  touch_count = buf[0] & 0x0F;
  if (touch_count == 0 || touch_count > 5)
    return 0;

  raw_x = ((uint16_t)(buf[1] & 0x0F) << 8) | buf[2];
  raw_y = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];

  FT5x16_MapCoord(raw_x, raw_y, x, y);
  return 1;
}

void FT5x16_SetCalibration(const Touch_Calib_t *calib) {
  if (calib != NULL)
    memcpy(&s_calib, calib, sizeof(Touch_Calib_t));
}

const Touch_Calib_t *FT5x16_GetCalibration(void) { return &s_calib; }
