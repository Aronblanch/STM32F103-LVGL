# LVGL v9.5.0 移植到 STM32F103ZETx 完整计划

## Context

当前STM32工程已具备完整的ILI9488 LCD驱动（FSMC Bank4 16位并口，320×480 RGB565）和FT5x16触摸驱动（I2C+BitBang）。目标是将 `D:\LVGL\lv_port_pc_vscode` 中的LVGL v9.5.0库及自定义UI（多语言Tab视图、按钮、滑块、下拉菜单、思源黑体中文字体）移植到此工程中，实现与PC端SDL模拟完全一致的渲染效果和触摸交互。

**硬件平台（已验证）：**
- MCU: STM32F103ZETx, 512KB Flash, 64KB内部RAM, 72MHz
- **外部SRAM: IS62WV51216BLL-55TLI, 1MB, FSMC Bank3 (NE3), 基地址 0x68000000** ✅ 已配置并测试通过
- LCD: ILI9488 320×480 RGB565, FSMC Bank4 (NE4, PG0=A10)
- Touch: FT5x16 I2C + BitBang fallback
- LVGL: v9.5.0（API与v8差异大，使用新的 `lv_display_create()` / `lv_indev_create()`）
- 颜色深度需从PC端32位改为16位RGB565

**外部SRAM内存布局规划（1MB总量）：**
- 0x68000000 ~ 0x68018FFF: 显示缓冲区1 (320×80×2 = 51,200 bytes)
- 0x6801A000 ~ 0x68032FFF: 显示缓冲区2 (320×80×2 = 51,200 bytes)
- 0x68040000 ~ 0x680BFFFF: LV_MEM 内存池 (512KB)
- 剩余空间预留给字体缓存和其他用途

---

## Task 1: 配置外部SRAM ✅ 已完成

**状态：已完成并验证通过**

**实际配置：**
- 芯片: IS62WV51216BLL-55TLI (512K×16bit = 1MB)
- FSMC Bank3 (NE3), 片选: PG10
- 基地址: `0x68000000`, 容量: 1,048,576字节
- 优化时序: ADDSET=4, DATAST=8 (基于55ns规格)
- 字节使能: NBL0(PE0)/NBL1(PE1) 标准模式
- 驱动文件: `Core/Src/IS62WV512.c` + `Core/Inc/IS62WV512.h`
- API: `SRAM_Init()`, `SRAM_WriteHalfWord()`, `SRAM_ReadHalfWord()`, `SRAM_WriteBuffer()`, `SRAM_ReadBuffer()`, `SRAM_Test()`
- 测试结果: 64KB区域地址线/数据线/棋盘格测试全部PASS

---

## Task 2: 复制LVGL源码并建立工程目录结构

**目标：** 将LVGL核心库文件复制到STM32工程中，排除PC平台特有文件。

**目标目录结构：**
```
d:\Day\stm32Demo\
├── Core/
│   ├── Inc/
│   │   ├── lv_port_disp.h      ← 新建：显示驱动适配
│   │   ├── lv_port_indev.h     ← 新建：输入设备适配
│   │   └── (现有头文件...)
│   └── Src/
│       ├── lv_port_disp.c      ← 新建：显示驱动适配实现
│       ├── lv_port_indev.c     ← 新建：输入设备适配实现
│       └── (现有源文件...)
├── Middlewares/
│   └── LVGL/
│       ├── lvgl/               ← 从D:\LVGL\lv_port_pc_vscode\lvgl\复制
│       │   ├── src/            ← 核心源码（全部保留）
│       │   ├── demos/          ← 演示程序（可选）
│       │   ├── examples/       ← 示例（可选，含移植模板）
│       │   ├── lvgl.h          ← 主头文件
│       │   ├── lv_version.h
│       │   └── CMakeLists.txt
│       └── lv_conf.h           ← 从PC项目复制并大幅修改
├── src/
│   ├── ui/
│   │   ├── ui_main.c           ← 从PC项目src/main.c提取UI代码
│   │   └── ui_main.h
│   └── font/
│       └── my_siyuan_font.c    ← 从PC项目src/font/复制
└── CMakeLists.txt              ← 修改：添加LVGL编译
```

**需要复制的文件（从 D:\LVGL\lv_port_pc_vscode）：**
- `lvgl/` 整个目录（排除 `.git/`）
- `src/font/my_siyuan_font.c` — 自定义中文字体
- `lv_conf.h` — 配置文件（作为修改基础）

**不需要复制的文件：**
- `SDL2-2.30.11/` — PC专用SDL库
- `src/hal/hal.c` — PC的SDL HAL
- `src/mouse_cursor_icon.c` — PC鼠标光标
- `src/freertos/`, `src/freertos_main.c` — FreeRTOS适配（暂不需要）

---

## Task 3: 适配 lv_conf.h 配置文件

**目标：** 将PC端32位/1MB配置修改为STM32 16位/外部RAM配置。

**关键修改项：**

| 配置项 | PC端值 | STM32目标值 | 说明 |
|--------|---------|-------------|------|
| `LV_COLOR_DEPTH` | 32 | **16** | RGB565匹配ILI9488 |
| `LV_MEM_SIZE` | 1048576 (1MB) | **524288 (512KB)** | 放在外部SRAM 0x68040000 |
| `LV_MEM_ADR` | 0 (自动) | **0x68040000** | 外部SRAM偏移256KB处 |
| `LV_DEF_REFR_PERIOD` | 33 | **33** | 保持30fps |
| `LV_DPI_DEF` | 130 | **130** | 保持一致 |
| `LV_USE_OS` | LV_OS_NONE | LV_OS_NONE | 不使用RTOS |
| `LV_USE_FS_STDIO` | 1 | **0** | STM32无标准文件系统 |
| `LV_USE_DRAW_SW` | 1 | 1 | 保持软件渲染 |
| `LV_FONT_MONTSERRAT_14` | 1 | 1 | 保留默认字体 |
| `LV_FONT_SOURCE_HAN_SANS_SC_16_CJK` | 1 | 1 | 保留中文字体 |
| `LV_USE_DEMO_WIDGETS` | 1 | **0** | 禁用不需要的Demo |
| `LV_USE_DEMO_BENCHMARK` | 1 | **0** | 节省Flash |

**字体策略：** 仅保留实际使用的字体（Montserrat 12/14、Source Han Sans SC 16），禁用其余所有字体以节省Flash（每个字体约20-80KB）。

**内存布局方案（IS62WV51216 1MB外部SRAM @ 0x68000000）：**
- 内部RAM (64KB): 栈(8KB) + 全局变量 + LVGL核心结构体
- 外部SRAM布局:
  - `0x68000000 ~ 0x6800C7FF`: 显示缓冲区1 (320×80×2 = 51,200 bytes)
  - `0x6800C800 ~ 0x68018FFF`: 显示缓冲区2 (320×80×2 = 51,200 bytes)
  - `0x68040000 ~ 0x680BFFFF`: LV_MEM 内存池 (512KB)
  - 剩余: 预留/扩展

---

## Task 4: 实现显示驱动适配层 (lv_port_disp.c/h)

**目标：** 创建LVGL v9 显示驱动，对接现有 `LCD_SetWindows()` + `LCD_WriteData()` API。

**核心实现（基于LVGL v9 API）：**

```c
/* lv_port_disp.c */
#include "lvgl.h"
#include "lcd.h"

// 显示缓冲区 - 直接指向外部SRAM地址（IS62WV51216 @ 0x68000000）
#define DISP_BUF_LINES  80  /* 每次刷新80行，6次覆盖全屏 */
#define DISP_BUF_SIZE   (320 * DISP_BUF_LINES * 2)  /* 51,200 bytes */

static uint8_t *buf1 = (uint8_t *)0x68000000;  /* 缓冲区1 */
static uint8_t *buf2 = (uint8_t *)0x6800C800;  /* 缓冲区2 */

static lv_display_t *disp;

/* flush回调 - 将LVGL渲染缓冲区写入LCD */
static void disp_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t *color_p = (uint16_t *)px_map;
    
    LCD_SetWindows(area->x1, area->y1, area->x2, area->y2);
    
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    for(uint32_t i = 0; i < size; i++) {
        LCD_WriteData(*color_p++);
    }
    
    lv_display_flush_ready(display);  /* 必须调用，通知LVGL刷新完成 */
}

void lv_port_disp_init(void)
{
    disp = lv_display_create(320, 480);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, DISP_BUF_SIZE, 
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}
```

**性能说明：**
- 80行缓冲 × 6次flush = 全屏刷新，比40行方案减少一半flush调用次数
- FSMC写时序83ns/pixel，320×80区域刷新时间 = 320×80×83ns ≈ 2.1ms/flush
- 全屏刷新 = 6×2.1ms ≈ 12.7ms，理论最大帧率约78fps

**后续优化方向：**
- 使用DMA批量传输替代逐像素写入（可将flush时间再减半）

---

## Task 5: 实现输入设备适配层 (lv_port_indev.c/h)

**目标：** 创建LVGL v9 触摸输入驱动，对接现有 `FT5x16_Scan()` API。

**核心实现：**

```c
/* lv_port_indev.c */
#include "lvgl.h"
#include "touch.h"

static lv_indev_t *indev_touchpad;

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
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read_cb);
}
```

**注意事项：**
- 触摸坐标已在 `FT5x16_Scan()` 内部经过校准映射，输出直接是LCD坐标
- 如果发现坐标偏移，调整 `Touch_Calib_t` 参数

---

## Task 6: 实现时钟处理（Tick集成）

**目标：** 为LVGL提供毫秒级时钟源。

**方案：利用 SysTick 中断（HAL已配置为1ms）**

```c
/* 在 stm32f1xx_it.c 的 SysTick_Handler 中添加 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    lv_tick_inc(1);  /* 每1ms递增LVGL tick */
}
```

**或者使用LVGL v9的tick回调注册方式：**
```c
/* 在初始化时注册 */
lv_tick_set_cb(HAL_GetTick);  /* LVGL直接使用HAL_GetTick获取毫秒数 */
```

推荐使用 `lv_tick_set_cb(HAL_GetTick)` 方式，更简洁且无需修改中断处理文件。

---

## Task 7: 修改 main.c 集成LVGL

**目标：** 在现有初始化流程中添加LVGL初始化和主循环调度。

**修改后的main.c结构：**

```c
/* USER CODE BEGIN Includes */
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui_main.h"  /* 自定义UI */
/* USER CODE END Includes */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_FSMC_Init();       /* LCD Bank4 + 外部SRAM Bank初始化 */
    MX_I2C1_Init();
    
    /* USER CODE BEGIN 2 */
    SRAM_Init();  /* 初始化外部SRAM时序（必须在LVGL前） */
    HAL_GPIO_WritePin(LCD_BG_GPIO_Port, LCD_BG_Pin, GPIO_PIN_SET);
    LCD_Init();
    FT5x16_Init();
    
    /* LVGL初始化 */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();
    lv_port_indev_init();
    
    /* 创建UI（从PC端移植的界面） */
    create_temp_ui();  /* 来自ui_main.c */
    /* USER CODE END 2 */
    
    while (1) {
        /* USER CODE BEGIN 3 */
        lv_timer_handler();  /* LVGL事件/重绘处理 */
        HAL_Delay(5);        /* 约200fps调度频率 */
        /* USER CODE END 3 */
    }
}
```

---

## Task 8: CMake构建系统集成

**目标：** 将LVGL源码编译纳入现有CMake构建系统。

**修改 `CMakeLists.txt`（根目录）：**

```cmake
# 添加LVGL相关定义
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)

# LVGL源文件（使用LVGL自带的CMakeLists.txt或手动列举）
add_subdirectory(Middlewares/LVGL/lvgl)

# 添加LVGL头文件路径
target_include_directories(stm32Demo PRIVATE
    Middlewares/LVGL
    Middlewares/LVGL/lvgl
    Middlewares/LVGL/lvgl/src
)

# 链接LVGL库
target_link_libraries(stm32Demo PRIVATE lvgl)

# 新增源文件
target_sources(stm32Demo PRIVATE
    Core/Src/lv_port_disp.c
    Core/Src/lv_port_indev.c
    src/ui/ui_main.c
    src/font/my_siyuan_font.c
)
```

**注意：** LVGL自带的CMakeLists.txt可能需要适配（它默认面向PC平台）。如果出现问题，可改为手动收集源文件：
```cmake
file(GLOB_RECURSE LVGL_SOURCES "Middlewares/LVGL/lvgl/src/*.c")
```

---

## Task 9: 移植PC端自定义UI代码

**目标：** 将 `D:\LVGL\lv_port_pc_vscode\src\main.c` 中的UI创建代码提取到 `src/ui/ui_main.c`。

**需要修改的部分：**
1. 移除所有SDL相关头文件和函数调用
2. 移除 `usleep()` 调用（替换为 `HAL_Delay()`）
3. 保留所有 `lv_*` API调用（控件创建、样式设置、事件回调）
4. 确保字体引用正确（`my_siyuan_font` 路径）
5. 检查是否使用了 `LV_USE_FS_STDIO` 相关的文件操作（需移除或替代）

**关键函数提取：**
- `create_temp_ui()` — 主UI创建函数
- 所有事件回调函数
- Observer相关代码

---

## Task 10: 链接脚本修改（堆栈扩展）

**目标：** 增大堆栈以满足LVGL需求。由于显示缓冲区和LV_MEM使用直接地址映射（0x68000000），无需在链接脚本中定义EXRAM段。

**修改 `STM32F103XX_FLASH.ld`：**

```ld
/* 堆栈调整 */
_Min_Heap_Size = 0x1000;   /* 4KB heap (原0x200) */
_Min_Stack_Size = 0x2000;  /* 8KB stack (原0x400) - LVGL需要较深栈 */
```

**说明：** 外部SRAM通过FSMC地址映射(0x68000000)直接访问，LVGL的`LV_MEM_ADR`和显示缓冲区指针都使用硬编码地址，无需链接器分配。

---

## 技术风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| ~~外部RAM未配置~~ | ~~阻塞~~ | ✅ 已解决：IS62WV51216 1MB SRAM 测试通过 |
| LVGL v9 API与模板不匹配 | 中 | 严格使用 `lv_display_create()` 等v9 API，参考 `lvgl/examples/porting/` |
| Flash不足（512KB放不下LVGL全部功能+字体） | 中 | 精简lv_conf.h，禁用不需要的Widget和字体，评估my_siyuan_font.c大小 |
| FSMC写入速度不够导致帧率低 | 低 | 写时序83ns/pixel，全屏约12.7ms，可满足30fps |
| 思源黑体字体Flash占用过大 | 中 | 检查字体文件大小（可能50-200KB），必要时裁减字符集 |
| LVGL CMakeLists.txt不兼容ARM交叉编译 | 中 | 备选方案：用file(GLOB_RECURSE)手动收集源文件 |
| 外部SRAM访问速度影响渲染性能 | 低 | SRAM读写时序69ns，远快于LCD刷新需求 |

---

## 验证方案

### 编译验证
```bash
cd d:\Day\stm32Demo\build
cmake --build . 2>&1 | tee build.log
# 检查：无错误，Flash占用 < 512KB，RAM占用（不含外部）< 64KB
```

### 功能验证检查点
1. **外部RAM验证：** 写入/回读测试Pattern（0xAA55交替）
2. **LVGL初始化：** `lv_init()` 无崩溃，串口打印版本号
3. **显示驱动：** `LCD_Clear(COLOR_WHITE)` 后调用 `lv_timer_handler()`，屏幕应显示LVGL默认背景
4. **触摸响应：** 触摸屏幕后LVGL控件有响应（按钮高亮、滑块移动）
5. **完整UI：** Tab切换正常，中文字体渲染正确，所有控件可交互
6. **性能：** 通过 `lv_demo_benchmark()` 测试帧率（目标 > 20fps）

### 烧录测试
- 使用OpenOCD/J-Link烧录ELF
- 串口(USART1)输出调试信息
- 观察LCD显示效果与PC端对比

---

## 执行顺序依赖

```
Task 1 ✅ (外部SRAM) ─→ Task 2 (复制LVGL源码) ─→ Task 3 (lv_conf.h) ─┐
                                                                        │
                         Task 10 (链接脚本/堆栈) ───────────────────────┤
                                                                        ▼
                                                  Task 4 (显示驱动) ──┐
                                                  Task 5 (输入驱动) ──┼─→ Task 7 (main.c集成)
                                                  Task 6 (Tick处理) ──┘        │
                                                                               ▼
                                                  Task 8 (CMake配置) ─→ Task 9 (UI移植) ─→ 编译验证
```

**下一步执行：** Task 2（复制LVGL源码）和 Task 10（链接脚本堆栈调整）可并行开始。
