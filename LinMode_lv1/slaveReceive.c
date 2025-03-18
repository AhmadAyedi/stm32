/* Includes */
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private variables */
UART_HandleTypeDef huart1;
volatile uint8_t lin_break_detected = 0;
uint8_t rx_buffer[10]; // Buffer for Break, Sync, PID, 5 data bytes, checksum (9 bytes total)
volatile uint8_t rx_index = 0;
volatile uint8_t receiving_frame = 0;

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void Process_LIN_Frame(void);
uint8_t Verify_Checksum(uint8_t *data, uint8_t len, uint8_t pid, uint8_t checksum);

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

    // Initial debug message
    printf("Slave initialized, waiting for LIN frame...\n");

    // Start receiving first byte (waiting for break)
    HAL_UART_Receive_IT(&huart1, &rx_buffer[rx_index], 1);

    while (1)
    {
        if (lin_break_detected)
        {
            printf("LIN Break detected, starting frame reception\n");
            lin_break_detected = 0;
            receiving_frame = 1;
            rx_index = 0;
            HAL_UART_Receive_IT(&huart1, &rx_buffer[rx_index], 1); // Restart reception
        }
        if (receiving_frame && rx_index >= 9)
        { // Wait for 9 bytes (0–8)
            Process_LIN_Frame();
            receiving_frame = 0;
            rx_index = 0;
            HAL_UART_Receive_IT(&huart1, &rx_buffer[rx_index], 1); // Prepare for next frame
        }
    }
}

/* UART Receive Complete Callback */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (receiving_frame)
        {
            printf("Received byte %d: 0x%02X\n", rx_index, rx_buffer[rx_index]);
            rx_index++;
            if (rx_index < 9)
            { // Expect 9 bytes: Break, Sync, PID, 5 data, Checksum
                HAL_UART_Receive_IT(&huart1, &rx_buffer[rx_index], 1);
            }
        }
        else
        {
            // Waiting for break, ignore data until break is detected
            HAL_UART_Receive_IT(&huart1, &rx_buffer[rx_index], 1);
        }
    }
}

/* UART Error Callback - LIN Break Detection */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_LBD) != RESET)
        {
            lin_break_detected = 1;
            __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_LBD); // Clear LIN Break flag
        }
    }
}

/* Process LIN Frame */
void Process_LIN_Frame(void)
{
    // Expected frame: [0x00 (Break), 0x55 (Sync), PID, Data[5], Checksum]
    if (rx_buffer[1] == 0x55)
    { // Sync field at index 1
        uint8_t pid = rx_buffer[2];
        uint8_t data[5];
        memcpy(data, &rx_buffer[3], 5);  // Data starts at index 3
        uint8_t checksum = rx_buffer[8]; // Checksum at index 8

        if (Verify_Checksum(data, 5, pid, checksum))
        {
            printf("Received LIN Frame - Sync: 0x%02X, PID: 0x%02X, Data: %c%c%c%c%c, Checksum: 0x%02X\n",
                   rx_buffer[1], pid, data[0], data[1], data[2], data[3], data[4], checksum);
        }
        else
        {
            printf("Checksum Error - Expected: 0x%02X, Received: 0x%02X\n",
                   (uint8_t)(~Verify_Checksum(data, 5, pid, 0)), checksum);
        }
    }
    else
    {
        printf("Sync Error - Expected 0x55, Received 0x%02X\n", rx_buffer[1]);
    }
}

/* Verify Checksum */
uint8_t Verify_Checksum(uint8_t *data, uint8_t len, uint8_t pid, uint8_t checksum)
{
    uint16_t sum = pid;
    for (uint8_t i = 0; i < len; i++)
    {
        sum += data[i];
        if (sum >= 256)
            sum -= 255;
    }
    return ((uint8_t)(~sum) == checksum);
}

/* System Clock Configuration */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
    huart1.Init.Mode = UART_MODE_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
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