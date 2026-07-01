/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fsmc.h"
#include "gpio.h"
#include "i2c.h"
#include "iwdg.h"
#include "usart.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "IS62WV512.h"
#include "lcd.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "touch.h"
#include "ui_main.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int __io_putchar(int ch) {
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
  return ch;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_FSMC_Init();
  MX_I2C1_Init();
  /* MX_IWDG_Init() 移到 USER CODE BEGIN 2 末尾，避免初始化期间看门狗超时 */
  /* USER CODE BEGIN 2 */
  printf("\r\n=== Boot Start ===\r\n");

  /* 初始化外部SRAM时序 */
  printf("[DBG] Before SRAM_Init\r\n");
  if (SRAM_Init() != HAL_OK) {
    printf("[SRAM] Init FAILED!\r\n");
    Error_Handler();
  }
  printf("[SRAM] Init OK\r\n");

  /* LCD初始化 */
  printf("[DBG] Before LCD_Init\r\n");
  HAL_GPIO_WritePin(LCD_BG_GPIO_Port, LCD_BG_Pin, GPIO_PIN_SET); /* 开启背光 */
  LCD_Init();
  LCD_Clear(0x0000); /* 清屏黑色 */
  printf("[LCD] Init OK\r\n");

  /* 触摸初始化 */
  printf("[DBG] Before Touch_Init\r\n");
  uint8_t touch_st = FT5x16_Init();
  if (touch_st == 0) {
    printf("[Touch] FT5x16 Init OK\r\n");
  } else {
    printf("[Touch] FT5x16 Init FAIL code=%u\r\n", touch_st);
  }

  /* LVGL初始化 */
  printf("[DBG] Before lv_init\r\n");
  lv_init();
  printf("[DBG] After lv_init, before tick_set_cb\r\n");
  lv_tick_set_cb(HAL_GetTick); /* 使用HAL_GetTick作为时钟源 */
  printf("[DBG] Before lv_port_disp_init\r\n");
  lv_port_disp_init();         /* 初始化显示驱动 */
  printf("[DBG] Before lv_port_indev_init\r\n");
  lv_port_indev_init();        /* 初始化触摸输入 */
  printf("[LVGL] Init OK, v%d.%d.%d\r\n", LVGL_VERSION_MAJOR,
         LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

  /* 创建自定义UI界面 */
  printf("[DBG] Before create_temp_ui\r\n");
  create_temp_ui();
  printf("=== Boot Complete ===\r\n");

  /* 所有初始化完成后才启动看门狗 */
  MX_IWDG_Init();
  printf("[IWDG] Watchdog started\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    lv_timer_handler();       /* LVGL事件/重绘处理 */
    HAL_IWDG_Refresh(&hiwdg); /* 喂狗 */
    HAL_Delay(5);             /* ~200Hz调度频率 */
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
   * @retval None
   */
  void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
      Error_Handler();
    }
  }

  /* USER CODE BEGIN 4 */

  /* USER CODE END 4 */

  /**
   * @brief  This function is executed in case of error occurrence.
   * @retval None
   */
  void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state
     */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
  }
#ifdef USE_FULL_ASSERT
  /**
   * @brief  Reports the name of the source file and the source line number
   *         where the assert_param error has occurred.
   * @param  file: pointer to the source file name
   * @param  line: assert_param error line source number
   * @retval None
   */
  void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */
    /* USER CODE END 6 */
  }
#endif /* USE_FULL_ASSERT */
