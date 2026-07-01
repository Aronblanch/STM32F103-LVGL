/**
  ******************************************************************************
  * @file    lcd.c
  * @brief   ILI9488 3.5" TFT LCD 驱动实现
  * @note    FSMC 16位并口, NE4(PG12) + A10(PG0)
  *          背光 PB0, 硬复位 PG15
  ******************************************************************************
  */

#include "lcd.h"
#include "fsmc.h"

/* ============================================================================ */
/*  FSMC 时序优化重配置                                                         */
/*                                                                              */
/*  CubeMX 默认生成的时序过于保守(ADDSET=15, DATAST=255)，且未启用扩展模式       */
/*  此函数在 LCD_Init 中调用，直接操作 FSMC 寄存器优化读写时序                    */
/*                                                                              */
/*  72MHz HCLK → 1个FSMC时钟 = 13.89ns                                         */
/*  ILI9488 写周期要求: 最小 66ns (根据数据手册)                                 */
/*  ILI9488 读周期要求: 最小 450ns (根据数据手册)                                */
/*                                                                              */
/*  写时序 (Mode A): ADDSET=1, DATAST=4 → (1+4+1)×13.89 = 83ns > 66ns ✓       */
/*  读时序 (Mode A): ADDSET=5, DATAST=28 → (5+28+1)×13.89 = 472ns > 450ns ✓   */
/* ============================================================================ */
static void LCD_FSMC_TimingConfig(void)
{
    /* 1. 禁用 Bank1 Region4 (清零 MBKEN 位) */
    FSMC_Bank1->BTCR[6] &= ~FSMC_BCRx_MBKEN;

    /* 2. 启用扩展模式 (EXTMOD=1)，使读写时序分离 */
    FSMC_Bank1->BTCR[6] |= FSMC_BCRx_EXTMOD;

    /* 3. 配置读时序 — BTR4 寄存器 */
    /*    ADDSET[3:0]=5, DATAST[7:0]=28, ACCMOD[1:0]=0 (Mode A) */
    FSMC_Bank1->BTCR[7] = (5U << 0)   |  /* ADDSET = 5 */
                           (28U << 8)  |  /* DATAST = 28 */
                           (0U << 28);    /* ACCMOD = Mode A */

    /* 4. 配置写时序 — BWTR4 寄存器 */
    /*    ADDSET[3:0]=1, DATAST[7:0]=4, ACCMOD[1:0]=0 (Mode A) */
    FSMC_Bank1E->BWTR[3] = (1U << 0)   |  /* ADDSET = 1 */
                            (4U << 8)   |  /* DATAST = 4 */
                            (0U << 28);    /* ACCMOD = Mode A */

    /* 5. 重新使能 Bank1 Region4 */
    FSMC_Bank1->BTCR[6] |= FSMC_BCRx_MBKEN;
}

/* ============================================================================ */
/*  基础读写函数                                                                */
/* ============================================================================ */

/**
  * @brief  向 LCD 写命令
  * @param  cmd: 16位命令值
  * @note   通过 FSMC 地址线 A10=0 的地址写入，LCD 此时接收命令
  */
void LCD_WriteCmd(uint16_t cmd)
{
    *LCD_CMD = cmd;
}

/**
  * @brief  向 LCD 写数据
  * @param  data: 16位数据值
  * @note   通过 FSMC 地址线 A10=1 的地址写入，LCD 此时接收数据/参数
  */
void LCD_WriteData(uint16_t data)
{
    *LCD_DATA = data;
}

/**
  * @brief  从 LCD 读取数据
  * @retval 读取到的16位数据
  * @note   用于读取 LCD ID 等信息，先写命令再读数据
  */
uint16_t LCD_ReadData(void)
{
    return *LCD_DATA;
}

/* ============================================================================ */
/*  ILI9488 初始化序列                                                          */
/*                                                                              */
/*  基于参考文件 ILI9488+CTC3.5_G22.txt 和 ILI9488 数据手册                     */
/*  初始化流程: 硬件复位 → Gamma校正 → 电源控制 → 像素格式 → 显示开启           */
/* ============================================================================ */

/**
  * @brief  LCD 初始化
  * @note   完整初始化流程:
  *         1. FSMC 时序优化
  *         2. 硬件复位 (PG15)
  *         3. Gamma 校正 (0xE0/0xE1)
  *         4. 电源控制 (0xC0/0xC1/0xC5)
  *         5. MADCTL 扫描方向 (0x36)
  *         6. 像素格式 RGB565 (0x3A = 0x55)
  *         7. 接口/帧率控制 (0xB0~0xB6)
  *         8. 其他设置 (0xE9/0xF7)
  *         9. 退出睡眠 (0x11)
  *         10. 开启显示 (0x29)
  */
void LCD_Init(void)
{
    /* --- 优化 FSMC 读写时序 --- */
    LCD_FSMC_TimingConfig();

    /* --- 硬件复位时序 (PG15) --- */
    /* 参考初始化代码要求: 高1ms → 低10ms → 高120ms */
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);     /* 必须延时 >= 10ms */
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(120);    /* 必须延时 >= 120ms，等待 LCD 内部初始化完成 */

    /* --- 正极性 Gamma 校正 (0xE0) --- */
    /* 数据手册 Section 7.27: Positive Gamma Correction */
    LCD_WriteCmd(ILI9488_PGAMCTRL);
    LCD_WriteData(0x00); LCD_WriteData(0x04); LCD_WriteData(0x0E);
    LCD_WriteData(0x08); LCD_WriteData(0x17); LCD_WriteData(0x0A);
    LCD_WriteData(0x40); LCD_WriteData(0x79); LCD_WriteData(0x4D);
    LCD_WriteData(0x07); LCD_WriteData(0x0E); LCD_WriteData(0x0A);
    LCD_WriteData(0x1A); LCD_WriteData(0x1D); LCD_WriteData(0x0F);

    /* --- 负极性 Gamma 校正 (0xE1) --- */
    /* 数据手册 Section 7.28: Negative Gamma Correction */
    LCD_WriteCmd(ILI9488_NGAMCTRL);
    LCD_WriteData(0x00); LCD_WriteData(0x1B); LCD_WriteData(0x1F);
    LCD_WriteData(0x02); LCD_WriteData(0x10); LCD_WriteData(0x05);
    LCD_WriteData(0x32); LCD_WriteData(0x34); LCD_WriteData(0x43);
    LCD_WriteData(0x02); LCD_WriteData(0x0A); LCD_WriteData(0x09);
    LCD_WriteData(0x33); LCD_WriteData(0x37); LCD_WriteData(0x0F);

    /* --- 电源控制1 (0xC0) --- */
    /* 数据手册 Section 7.9: Power Control 1 */
    /* 参数1: 0x18 = VCI=2.8V, VRH[5:0]=24 */
    /* 参数2: 0x16 = VCI1=2.5V, BT[2:0]=6 */
    LCD_WriteCmd(ILI9488_PWCTR1);
    LCD_WriteData(0x18);
    LCD_WriteData(0x16);

    /* --- 电源控制2 (0xC1) --- */
    /* 数据手册 Section 7.10: Power Control 2 */
    /* 0x41: VC1[5:0] 设定 */
    LCD_WriteCmd(ILI9488_PWCTR2);
    LCD_WriteData(0x41);

    /* --- VCOM 控制 (0xC5) --- */
    /* 数据手册 Section 7.12: VCOM Control */
    LCD_WriteCmd(ILI9488_VMCTR1);
    LCD_WriteData(0x00);
    LCD_WriteData(0x22);   /* VCM=0x22 */
    LCD_WriteData(0x80);   /* VDV=0x80 */

    /* --- 存储器访问控制 (0x36) --- */
    /* 数据手册 Section 7.19: Memory Access Control */
    /* 0x08: BGR=1 (红蓝互换，适配多数3.5寸屏的颜色滤波排列) */
    /*       MY=0, MX=0, MV=0 → 竖屏默认方向 */
    /* 避坑: 若红蓝色互换，将 BGR 位清零改为 0x00 */
    LCD_WriteCmd(ILI9488_MADCTL);
    LCD_WriteData(0x08);

    /* --- 像素格式设置 (0x3A) --- */
    /* 数据手册 Section 7.20: Interface Pixel Format */
    /* 0x55 = DPI[6:4]=101(RGB565), DBI[2:0]=101(RGB565) */
    /*                                                                        */
    /* 避坑说明: 参考初始化代码使用 0x66 (RGB666 18位色)，但在16位并口下:       */
    /*   - RGB666: 每像素需2次16位写入（高8位+低8位），刷屏速率减半            */
    /*   - RGB565: 每像素仅需1次16位写入，效率翻倍，且STM32生态通用性最好       */
    /*   - RGB565的5位红/蓝→6位内部映射时LSB填0，颜色精度轻微降低，可忽略       */
    LCD_WriteCmd(ILI9488_PIXFMT);
    LCD_WriteData(0x55);

    /* --- 接口模式控制 (0xB0) --- */
    /* 数据手册 Section 7.30: Interface Mode Control */
    /* 0x00: RGB接口模式禁用，使用MCU接口 */
    LCD_WriteCmd(ILI9488_IFMODE);
    LCD_WriteData(0x00);

    /* --- 帧率控制 (0xB1) --- */
    /* 数据手册 Section 7.31: Frame Rate Control */
    /* 0xB0: 帧率约70Hz (根据RTNA设置) */
    LCD_WriteCmd(ILI9488_FRMCTR1);
    LCD_WriteData(0xB0);

    /* --- 反转控制 (0xB4) --- */
    /* 数据手册 Section 7.32: Display Inversion Control */
    /* 0x02: Column inversion (减少串扰) */
    LCD_WriteCmd(ILI9488_INVCTR);
    LCD_WriteData(0x02);

    /* --- 显示功能控制 (0xB6) --- */
    /* 数据手册 Section 7.38: RGB/MCU Interface Control */
    LCD_WriteCmd(ILI9488_DFUNCTR);
    LCD_WriteData(0x02);
    LCD_WriteData(0x22);

    /* --- 设置图像模式 (0xE9) --- */
    /* 0x00: 正常图像模式 */
    LCD_WriteCmd(ILI9488_SETIMAGE);
    LCD_WriteData(0x00);

    /* --- 自适应更新 (0xF7) --- */
    LCD_WriteCmd(ILI9488_ADAPTIVE);
    LCD_WriteData(0xA9);
    LCD_WriteData(0x51);
    LCD_WriteData(0x2C);
    LCD_WriteData(0x82);

    /* --- 退出睡眠模式 (0x11) --- */
    /* 数据手册 Section 7.5: Sleep Out */
    /* 发送后必须延时 >= 120ms，等待内部DC-DC和振荡器稳定 */
    LCD_WriteCmd(ILI9488_SLEEPOUT);
    HAL_Delay(120);

    /* --- 开启显示 (0x29) --- */
    /* 数据手册 Section 7.4: Display On */
    LCD_WriteCmd(ILI9488_DISPON);

    /* --- 避坑: 若颜色反转(白变黑/黑变白)，取消下面注释启用颜色反转 --- */
    /* LCD_WriteCmd(ILI9488_INVON); */
}

/* ============================================================================ */
/*  窗口与绘图函数                                                              */
/* ============================================================================ */

/**
  * @brief  设置 LCD 显示窗口
  * @param  x1: 起始列地址 (0~319)
  * @param  y1: 起始行地址 (0~479)
  * @param  x2: 结束列地址 (0~319)
  * @param  y2: 结束行地址 (0~479)
  * @note   设置后可直接连续写入像素数据，地址自动递增
  */
void LCD_SetWindows(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* 列地址设置 (0x2A): SC[15:0], EC[15:0] */
    LCD_WriteCmd(ILI9488_CASET);
    LCD_WriteData(x1 >> 8);     /* SC 高8位 */
    LCD_WriteData(x1 & 0xFF);   /* SC 低8位 */
    LCD_WriteData(x2 >> 8);     /* EC 高8位 */
    LCD_WriteData(x2 & 0xFF);   /* EC 低8位 */

    /* 行地址设置 (0x2B): SP[15:0], EP[15:0] */
    LCD_WriteCmd(ILI9488_PASET);
    LCD_WriteData(y1 >> 8);     /* SP 高8位 */
    LCD_WriteData(y1 & 0xFF);   /* SP 低8位 */
    LCD_WriteData(y2 >> 8);     /* EP 高8位 */
    LCD_WriteData(y2 & 0xFF);   /* EP 低8位 */

    /* 写显示内存 (0x2C): 后续写入的数据将被视为像素颜色 */
    LCD_WriteCmd(ILI9488_RAMWR);
}

/**
  * @brief  在指定坐标画一个点
  * @param  x: 列坐标 (0~319)
  * @param  y: 行坐标 (0~479)
  * @param  color: RGB565 颜色值
  */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetWindows(x, y, x, y);
    LCD_WriteData(color);
}

/**
  * @brief  全屏清屏
  * @param  color: RGB565 颜色值
  * @note   设置全屏窗口后连续写入 320×480 个像素
  *         每个像素一次16位写入，共 153600 次写入
  */
void LCD_Clear(uint16_t color)
{
    uint32_t i;
    uint32_t total = (uint32_t)LCD_WIDTH * LCD_HEIGHT;

    LCD_SetWindows(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    for (i = 0; i < total; i++)
    {
        LCD_WriteData(color);
    }
}
