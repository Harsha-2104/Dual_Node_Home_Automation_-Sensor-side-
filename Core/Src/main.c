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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <math.h>

// Import the specific library you downloaded
#include "SX1278.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Structure to hold data for one appliance
typedef struct {
    uint32_t adcChannel;       // ADC Channel (ADC_CHANNEL_0, etc.)
    GPIO_TypeDef* relayPort;   // GPIO Port for Relay
    uint16_t relayPin;         // GPIO Pin for Relay
    double current;            // RMS Current (Amps)
    double power;              // Power (Watts)
    double totalEnergyWs;      // Total Energy (Watt-Seconds/Joules)
} Appliance_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// --- Calibration & Config ---
const float calibrationFactor = 60.6; // For 33ohm burden resistor + SCT-013-100
const float supplyVoltage = 230.0;    // Fixed AC Voltage (Replace with sensor later if needed)
const int adcResolution = 4096;
const int offset = 2048;              // 1.65V DC Offset center point

// --- Runtime Variables ---
Appliance_t appliances[4];            // Array for 4 appliances
int currentTxIndex = 0;               // Tracks which appliance is sending data (0-3)
uint32_t lastTxTime = 0;              // Timer for LoRa loop
uint32_t lastEnergyTime = 0;          // Timer for Energy integration

// --- LoRa Variables ---
SX1278_hw_t loraHW;                   // Hardware struct (pins)
SX1278_t lora;                        // Logic struct (state)
char msgBuffer[100];                  // Buffer for UART logging
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void Init_Appliances(void);
void ADC_Select_Channel(uint32_t channel);
double Calculate_RMS(uint32_t channel, int samples);
void Handle_LoRa_Command(char* cmd);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 1. Initialize the Appliance Structs with correct Pins/Channels
void Init_Appliances(void) {
    // Appliance 1: PA0 (ADC) & PB12 (Relay)
    appliances[0].adcChannel = ADC_CHANNEL_0;
    appliances[0].relayPort = GPIOB;
    appliances[0].relayPin = GPIO_PIN_12;

    // Appliance 2: PA1 (ADC) & PB13 (Relay)
    appliances[1].adcChannel = ADC_CHANNEL_1;
    appliances[1].relayPort = GPIOB;
    appliances[1].relayPin = GPIO_PIN_13;

    // Appliance 3: PA2 (ADC) & PB14 (Relay)
    appliances[2].adcChannel = ADC_CHANNEL_2;
    appliances[2].relayPort = GPIOB;
    appliances[2].relayPin = GPIO_PIN_14;

    // Appliance 4: PA3 (ADC) & PB15 (Relay)
    appliances[3].adcChannel = ADC_CHANNEL_3;
    appliances[3].relayPort = GPIOB;
    appliances[3].relayPin = GPIO_PIN_15;
}

// 2. Helper to switch the ADC MUX
void ADC_Select_Channel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

// 3. RMS Calculation Logic
double Calculate_RMS(uint32_t channel, int samples) {
    ADC_Select_Channel(channel);

    long sumSquared = 0;
    for (int i = 0; i < samples; i++) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        int signal = HAL_ADC_GetValue(&hadc1);

        long filtered = signal - offset;

        // Zero-Cross Noise Gate: Ignore tiny fluctuations near zero
        if (filtered > -5 && filtered < 5) filtered = 0;

        sumSquared += (filtered * filtered);

        // Short delay to spread samples over the AC wave
        // 50 loops is roughly 10-20us depending on clock
        for(int k=0; k<50; k++) __NOP();
    }

    double meanSquare = (double)sumSquared / samples;
    double rmsADC = sqrt(meanSquare);
    double rmsVolts = rmsADC * (3.3 / (float)adcResolution); // Assuming 3.3V VREF
    double amps = rmsVolts * calibrationFactor;

    // Low current cutoff (software noise floor)
    if (amps < 0.15) return 0.0;

    return amps;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // A. Initialize Appliance Structs
  Init_Appliances();
  lastEnergyTime = HAL_GetTick();
  lastTxTime = HAL_GetTick();

  // B. Initialize LoRa Hardware Struct (Connecting Pins to Driver)
  loraHW.dio0.port = LORA_DIO0_GPIO_Port;
  loraHW.dio0.pin = LORA_DIO0_Pin;
  loraHW.nss.port = LORA_NSS_GPIO_Port;
  loraHW.nss.pin = LORA_NSS_Pin;
  loraHW.reset.port = LORA_RST_GPIO_Port;
  loraHW.reset.pin = LORA_RST_Pin;
  loraHW.spi = &hspi1; // Link the SPI Handle

  // C. Link HW to Logic
  lora.hw = &loraHW;

  // D. Start LoRa
  // Freq: 433MHz, Power: 17dBm, SF: 7, BW: 125kHz
  SX1278_init(&lora, 433000000, SX1278_POWER_17DBM, SX1278_LORA_SF_7,
              SX1278_LORA_BW_125KHZ, SX1278_LORA_CR_4_5, SX1278_LORA_CRC_EN, 10);

  // Enter Receive Mode immediately to listen for commands
  SX1278_LoRaEntryRx(&lora, 16, 2000);

  HAL_UART_Transmit(&huart1, (uint8_t*)"System Started\r\n", 16, 100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // --- TASK 1: Energy Measurement (Runs Continuously) ---
	  // --- COMBINED TASK: Measure & Transmit (Every 250ms) ---
	      // 250ms * 4 Devices = 1 Second total cycle time
	      if (HAL_GetTick() - lastTxTime >= 250) {
	          lastTxTime = HAL_GetTick();

	          // 1. Point to the specific appliance we are updating NOW
	          Appliance_t *app = &appliances[currentTxIndex];

	          // 2. Measure RMS only for this device (Saves huge amount of simulation CPU)
	          // We assume 0.25s has passed since last check for energy math
	          app->current = Calculate_RMS(app->adcChannel, 50);
	          app->power = app->current * supplyVoltage;
	          app->totalEnergyWs += (app->power * 0.25);

	          double energyKWh = app->totalEnergyWs / 3600000.0;

	          // 3. Manual Float Conversion (Fixes the "Empty Value" bug)
	          // Split Current (I)
	          int iInt = (int)app->current;
	          int iDec = (int)((app->current - iInt) * 100);

	          // Split Power (P)
	          int pInt = (int)app->power;
	          int pDec = (int)((app->power - pInt) * 10);

	          // Split Energy (E)
	          int eInt = (int)energyKWh;
	          int eDec = (int)((energyKWh - eInt) * 10000);

	          // 4. Format Packet (Using %d instead of %f)
	          char loraPacket[64];
	          sprintf(loraPacket, "ID:%d,V:%d,I:%d.%02d,P:%d.%d,E:%d.%04d",
	                  currentTxIndex + 1,
	                  (int)supplyVoltage,
	                  iInt, iDec,
	                  pInt, pDec,
	                  eInt, eDec);

	          // 5. Send Packet
	          SX1278_LoRaEntryTx(&lora, 16, 2000);
	          SX1278_LoRaTxPacket(&lora, (uint8_t*)loraPacket, strlen(loraPacket), 2000);

	          // 6. Debug Print to Serial
	          char debugBuf[100];
	          sprintf(debugBuf, "[TX] %s\r\n", loraPacket);
	          HAL_UART_Transmit(&huart1, (uint8_t*)debugBuf, strlen(debugBuf), 100);

	          // 7. Go back to RX Mode & Increment Index
	          SX1278_LoRaEntryRx(&lora, 16, 2000);
	          currentTxIndex++;
	          if(currentTxIndex > 3) currentTxIndex = 0;
	      }

    // --- TASK 3: LoRa Reception (Poll DIO0) ---
    // If DIO0 is High, a packet has arrived
    if (HAL_GPIO_ReadPin(LORA_DIO0_GPIO_Port, LORA_DIO0_Pin) == GPIO_PIN_SET) {
        int len = SX1278_LoRaRxPacket(&lora);
        if (len > 0) {

        	char *rxData = (char*)lora.rxBuffer;
			rxData[len] = 0; // Ensure Null termination for string safety

			// Print Received Command
			char debugBuf[300];
			sprintf(debugBuf, "[RX CMD] %s\r\n", rxData);
			HAL_UART_Transmit(&huart1, (uint8_t*)debugBuf, strlen(debugBuf), 100);

			// Process Command
			Handle_LoRa_Command(rxData);
        }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

// --- LoRa Command Parser ---
// Expects: "R1:0" (Relay 1 OFF), "R2:1" (Relay 2 ON)
void Handle_LoRa_Command(char* cmd) {
    int relayID, state;
    // Scan string for pattern "R<number>:<number>"
    if (sscanf(cmd, "R%d:%d", &relayID, &state) == 2) {
        if (relayID >= 1 && relayID <= 4) {
            Appliance_t *target = &appliances[relayID - 1];

            // Logic: 1 = ON (Reset Pin), 0 = OFF (Set Pin)
            // Assuming Active LOW Relay Module. Reverse if Active HIGH.
            if (state == 1) {
                HAL_GPIO_WritePin(target->relayPort, target->relayPin, GPIO_PIN_RESET);
            } else {
                HAL_GPIO_WritePin(target->relayPort, target->relayPin, GPIO_PIN_SET);
            }
        }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LORA_NSS_Pin, GPIO_PIN_SET); // Set CS High (Inactive)

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LORA_RST_Pin|RELAY1_Pin|RELAY2_Pin|RELAY3_Pin
                          |RELAY4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LORA_NSS_Pin */
  GPIO_InitStruct.Pin = LORA_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LORA_NSS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LORA_DIO0_Pin */
  GPIO_InitStruct.Pin = LORA_DIO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LORA_DIO0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LORA_RST_Pin RELAY1_Pin RELAY2_Pin RELAY3_Pin RELAY4_Pin */
  GPIO_InitStruct.Pin = LORA_RST_Pin|RELAY1_Pin|RELAY2_Pin|RELAY3_Pin
                          |RELAY4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// UART GPIO INIT (Needed for Wokwi sometimes if it drops it)
//void HAL_UART_MspInit(UART_HandleTypeDef* huart)
//{
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
//  if(huart->Instance==USART1)
//  {
//    __HAL_RCC_USART1_CLK_ENABLE();
//    __HAL_RCC_GPIOA_CLK_ENABLE();
//    // PA9 -> TX
//    GPIO_InitStruct.Pin = GPIO_PIN_9;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//    // PA10 -> RX
//    GPIO_InitStruct.Pin = GPIO_PIN_10;
//    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//  }
//}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
