/* Includes */
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private variables */
UART_HandleTypeDef huart1;

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void Send_LIN_Frame(void);                                         // Added prototype
uint8_t Compute_Parity(uint8_t id);                                // Added prototype
uint8_t Compute_Checksum(uint8_t *data, uint8_t len, uint8_t pid); // Added prototype

/* SWV printf redirection */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
PUTCHAR_PROTOTYPE
{
    ITM_SendChar(ch);
    return ch;
}

/* Main function */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    while (1)
    {
        Send_LIN_Frame();
        HAL_Delay(1000); // Send every 1 second
    }
}

/* Send LIN Frame */
void Send_LIN_Frame(void)
{
    uint8_t break_field = 0x00;                      // Break (low pulse)
    uint8_t sync_field = 0x55;                       // Sync
    uint8_t id = 0x01;                               // Example ID (1)
    uint8_t pid = id | (Compute_Parity(id) << 6);    // PID with parity
    uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello" in ASCII
    uint8_t checksum = Compute_Checksum(data, sizeof(data), pid);

    // Send Break (emulate 13-bit low by sending 0x00 with break detection)
    HAL_LIN_SendBreak(&huart1);
    HAL_UART_Transmit(&huart1, &break_field, 1, HAL_MAX_DELAY);

    // Send Sync
    HAL_UART_Transmit(&huart1, &sync_field, 1, HAL_MAX_DELAY);

    // Send PID
    HAL_UART_Transmit(&huart1, &pid, 1, HAL_MAX_DELAY);

    // Send Data
    HAL_UART_Transmit(&huart1, data, sizeof(data), HAL_MAX_DELAY);

    // Send Checksum
    HAL_UART_Transmit(&huart1, &checksum, 1, HAL_MAX_DELAY);

    // Display on SWV
    printf("Sent LIN Frame - Break: 0x00, Sync: 0x%02X, PID: 0x%02X, Data: Hello, Checksum: 0x%02X\n",
           sync_field, pid, checksum);
}

/* Compute Parity for PID */
uint8_t Compute_Parity(uint8_t id)
{
    uint8_t p0 = (id & 0x01) ^ ((id >> 1) & 0x01) ^ ((id >> 2) & 0x01) ^ ((id >> 4) & 0x01);
    uint8_t p1 = ~((id >> 1) & 0x01) ^ ((id >> 3) & 0x01) ^ ((id >> 4) & 0x01) ^ ((id >> 5) & 0x01);
    return (p0 & 0x01) | ((p1 & 0x01) << 1);
}

/* Compute Checksum (Enhanced: includes PID) */
uint8_t Compute_Checksum(uint8_t *data, uint8_t len, uint8_t pid)
{
    uint16_t sum = pid;
    for (uint8_t i = 0; i < len; i++)
    {
        sum += data[i];
        if (sum >= 256)
            sum -= 255;
    }
    return (uint8_t)(~sum);
}

/* System Clock Configuration */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 40;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USART1 Initialization */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 19200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_LIN_INIT;
    huart1.AdvancedInit.LINBreakDetectLength = UART_LINBREAKDETECTLENGTH_11B;
    if (HAL_LIN_Init(&huart1, UART_LINBREAKDETECTLENGTH_11B) != HAL_OK)
    {
        Error_Handler();
    }
}

/* GPIO Initialization */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

/* Error Handler */
void Error_Handler(void)
{
    while (1)
    {
    }
}