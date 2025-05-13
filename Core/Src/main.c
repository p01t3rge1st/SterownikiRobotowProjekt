/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include <stdio.h>
//#include "uart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

extern I2C_HandleTypeDef hi2c3;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DHT11_COM_Pin GPIO_PIN_4
#define DHT11_COM_GPIO_Port GPIOA

#define DHT11_BIT_DELAY_US         40
#define DHT11_POST_INPUT_DELAY_US  10
#define DHT11_RESPONSE_DELAY1_US   40
#define DHT11_RESPONSE_DELAY2_US   100
#define DHT11_RESPONSE_DELAY3_US   40   

#define SGP40_I2C_ADDR (0x59 << 1) 
#define SGP30_I2C_ADDR (0x58 << 1) 

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;
//TIM_HandleTypeDef htim1;

UART_HandleTypeDef husart2;
//I2C_HandleTypeDef hi2c3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes ------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
void DHT11_Start(void);
uint8_t DHT11_CheckResponse(void);
uint8_t DHT11_ReadByte(void);
uint8_t sgp40_crc(uint8_t *data, uint8_t len);
HAL_StatusTypeDef SGP40_MeasureRaw(uint16_t *voc_raw);
HAL_StatusTypeDef SGP30_Init(void);
HAL_StatusTypeDef SGP30_Read(uint16_t *co2eq, uint16_t *tvoc);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;
uint16_t RH, TEMP;
uint8_t checksum;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_USART2_UART_Init();

    HAL_TIM_Base_Start(&htim1);
    MX_I2C3_Init();

    SGP30_Init();
    HAL_Delay(20);

    while (1)
    {
        DHT11_Start();
        if (DHT11_CheckResponse())
        {
            Rh_byte1 = DHT11_ReadByte();
            Rh_byte2 = DHT11_ReadByte();
            Temp_byte1 = DHT11_ReadByte();
            Temp_byte2 = DHT11_ReadByte();
            checksum = DHT11_ReadByte();

            uint16_t raw_hum = (Rh_byte1 << 8) | Rh_byte2;
            int16_t raw_temp = (Temp_byte1 << 8) | Temp_byte2;
            float humidity = raw_hum / 10.0f;
            float temperature = raw_temp / 10.0f;

            
            if (raw_temp & 0x8000)
                temperature = -((raw_temp & 0x7FFF) / 10.0f);

            //char dbg[64];
            //snprintf(dbg, sizeof(dbg), "R1:%d R2:%d T1:%d T2:%d C:%d\r\n", Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2, checksum);
            //HAL_UART_Transmit(&husart2, (uint8_t*)dbg, strlen(dbg), HAL_MAX_DELAY);

            if (((Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2) & 0xFF) == checksum)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "Temp: %.1f C, RH: %.1f %%\r\n", temperature, humidity);
                HAL_UART_Transmit(&husart2, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY);
            }
            else
            {
                char err[] = "Błąd sumy kontrolnej DHT22\r\n";
                HAL_UART_Transmit(&husart2, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
            }
        }
        else
        {
            char err[] = "Brak odpowiedzi z DHT22\r\n";
            HAL_UART_Transmit(&husart2, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
        }

        uint16_t co2, tvoc;
        if (SGP30_Read(&co2, &tvoc) == HAL_OK)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "SGP30: CO2eq: %u ppm, TVOC: %u ppb\r\n", co2, tvoc);
            HAL_UART_Transmit(&husart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
        }
        else
        {
            char err[] = "SGP30 error\r\n";
            HAL_UART_Transmit(&husart2, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
        }
        HAL_Delay(1000);
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 170-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */

static void MX_USART2_UART_Init(void)
{
  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  husart2.Instance = USART2;
  husart2.Init.BaudRate = 115200;
  husart2.Init.WordLength = UART_WORDLENGTH_8B;
  husart2.Init.StopBits = UART_STOPBITS_1;
  husart2.Init.Parity = UART_PARITY_NONE;
  husart2.Init.Mode = UART_MODE_TX_RX;
  husart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  husart2.Init.OverSampling = UART_OVERSAMPLING_16;
  husart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  if (HAL_UART_Init(&husart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 (USART2 TX/RX) */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < us);
}

void DHT11_Start(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_COM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_COM_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(DHT11_COM_GPIO_Port, DHT11_COM_Pin, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(DHT11_COM_GPIO_Port, DHT11_COM_Pin, GPIO_PIN_SET);
    delay_us(20);

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_COM_GPIO_Port, &GPIO_InitStruct);

    delay_us(DHT11_POST_INPUT_DELAY_US);
}

uint8_t DHT11_CheckResponse(void)
{
    uint8_t response = 0;
    delay_us(DHT11_RESPONSE_DELAY1_US);
    if (!HAL_GPIO_ReadPin(DHT11_COM_GPIO_Port, DHT11_COM_Pin))
    {
        delay_us(DHT11_RESPONSE_DELAY2_US);
        if (HAL_GPIO_ReadPin(DHT11_COM_GPIO_Port, DHT11_COM_Pin)) response = 1;
        delay_us(DHT11_RESPONSE_DELAY3_US);
    }
    return response;
}

uint8_t DHT11_ReadByte(void)
{
    uint8_t j, byte = 0;
    for (j = 0; j < 8; j++)
    {
        while (!HAL_GPIO_ReadPin(DHT11_COM_GPIO_Port, DHT11_COM_Pin));
        delay_us(DHT11_BIT_DELAY_US);
        if (HAL_GPIO_ReadPin(DHT11_COM_GPIO_Port, DHT11_COM_Pin))
            byte |= (1 << (7 - j));
        while (HAL_GPIO_ReadPin(DHT11_COM_GPIO_Port, DHT11_COM_Pin));
    }
    return byte;
}

HAL_StatusTypeDef SGP30_Init(void)
{
    uint8_t cmd[2] = {0x20, 0x03};
    return HAL_I2C_Master_Transmit(&hi2c3, SGP30_I2C_ADDR, cmd, 2, HAL_MAX_DELAY);
}

HAL_StatusTypeDef SGP30_Read(uint16_t *co2eq, uint16_t *tvoc)
{
    uint8_t cmd[2] = {0x20, 0x08};
    if (HAL_I2C_Master_Transmit(&hi2c3, SGP30_I2C_ADDR, cmd, 2, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(15); 

    uint8_t rx[6];
    if (HAL_I2C_Master_Receive(&hi2c3, SGP30_I2C_ADDR, rx, 6, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    *co2eq = (rx[0] << 8) | rx[1];
    *tvoc  = (rx[3] << 8) | rx[4];
    return HAL_OK;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
