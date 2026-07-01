---
name: stm32-cubemx-workflow
description: STM32CubeMX项目的标准开发工作流程。当需求涉及STM32外设功能（GPIO、EXTI、UART、SPI、定时器等）时自动应用，确保先确认CubeMX配置、再验证生成代码、最后仅在USER CODE区域开发。
---

# STM32CubeMX 项目开发工作流程

## 适用场景

当需求涉及任何 STM32 外设功能时使用此 skill，包括但不限于：
- GPIO 配置（输入/输出/外部中断）
- USART/UART 串口通信
- SPI/I2C 通信
- 定时器/PWM
- ADC/DAC
- DMA
- 其他任何需要 CubeMX 图形化配置的外设

## 核心原则

**底层驱动代码由 CubeMX 生成，用户代码仅限 USER CODE 区域。**

CubeMX 生成的代码（如 gpio.c、usart.c、tim.c 及对应的 .h 文件）每次重新生成都会覆盖非 USER CODE 区域的内容。因此：
- 绝不修改 CubeMX 生成的代码区域
- 所有用户逻辑必须写在 `/* USER CODE BEGIN xxx */` 和 `/* USER CODE END xxx */` 之间

## 三步开发流程

### 第一步：确认 CubeMX 配置

**必须先向用户确认是否已在 CubeMX 中完成相关外设配置。**

使用 `ask_user_question` 工具提问，列出需要配置的具体外设和参数，例如：

```
你是否已经在 STM32CubeMX 中完成了以下配置并重新生成了代码？
1. PA0 (LED1) 配置为 GPIO_Output
2. PF8 (KEY3) 配置为 GPIO_EXTI8，上升沿触发
3. USART1 配置为异步模式，波特率 115200
```

选项至少包括：
- "已配置完成" — 用户已完成，进入第二步
- "尚未配置" — 提示用户先去 CubeMX 配置，暂停开发

### 第二步：验证生成代码

用户确认后，读取相关文件二次确认 CubeMX 配置是否真正生效：

**必检文件：**
| 文件 | 检查内容 |
|------|----------|
| `Core/Inc/main.h` | 引脚宏定义（Pin、GPIO_Port、EXTI_IRQn 等）是否存在 |
| `Core/Src/gpio.c` | GPIO/EXTI 初始化配置是否正确（模式、上下拉、中断优先级） |
| `Core/Src/stm32f1xx_it.c` | 对应的中断处理函数是否已生成 |
| `Core/Src/<peripheral>.c` | 相关外设（如 usart.c、tim.c）初始化函数是否存在 |
| `Core/Inc/<peripheral>.h` | 相关外设头文件函数声明是否存在 |

验证要点：
- 宏定义名称是否匹配（如 CubeMX 命名为 SW3 而用户需求写 KEY3，两者引脚一致即可）
- GPIO 模式是否正确（如 `GPIO_MODE_IT_RISING` 对应上升沿触发）
- 中断向量是否已注册（如 EXTI9_5_IRQn 对应 PF8 的 EXTI8）
- 外设初始化函数是否在 main.c 中被调用

### 第三步：在 USER CODE 区域开发

验证通过后，仅在 USER CODE 区域编写业务逻辑：

**常用 USER CODE 位置：**

| 文件 | USER CODE 区域 | 用途 |
|------|----------------|------|
| `Core/Src/main.c` | `USER CODE BEGIN 4` | EXTI回调、外设回调函数 |
| `Core/Src/main.c` | `USER CODE BEGIN/END 3` | 主循环逻辑 |
| `Core/Src/main.c` | `USER CODE BEGIN 2` | 初始化后的额外设置 |
| `Core/Src/main.c` | `USER CODE BEGIN Includes` | 添加自定义头文件 |
| `Core/Src/main.c` | `USER CODE BEGIN PV` | 添加全局变量 |
| `Core/Src/stm32f1xx_it.c` | `USER CODE BEGIN EXTI_x_IRQn 0/1` | 中断前/后处理 |
| `Core/Inc/main.h` | `USER CODE BEGIN Private defines` | 自定义宏定义 |
| `Core/Inc/main.h` | `USER CODE BEGIN Includes` | 自定义头文件引用 |

**EXTI 中断回调示例：**
```c
/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == SW3_Pin)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
  }
}
/* USER CODE END 4 */
```

## 注意事项

- CubeMX 中的引脚命名可能与需求文档不同（如 SW3 vs KEY3），开发时必须使用 CubeMX 生成的宏定义名（如 `SW3_Pin`）
- STM32F1xx 的 EXTI 中断向量映射：EXTI0~4 各有独立 IRQn，EXTI5~9 共用 EXTI9_5_IRQn，EXTI10~15 共用 EXTI15_10_IRQn
- 每次重新在 CubeMX 中生成代码后，应重新执行第二步验证，确认新配置生效