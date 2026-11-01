#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "misc.h"

// Log_Print using USART1
void Log_Print(const char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

void AppInit(void) {
    // Basic system init is handled by the emulator, but we need to enable peripheral clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
}

void UART1_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // Configure USART1 Tx (PA.09) as alternate function push-pull
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Configure USART1 Rx (PA.10) as input floating
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART1 configuration
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

void UART2_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // Configure USART2 Tx (PA.02) as alternate function push-pull
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Configure USART2 Rx (PA.03) as input floating
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // USART2 configuration
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);

    // Enable the USART2 Receive Interrupt
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // Enable the USART2 Interrupt in the NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

// Global buffer for RX
volatile uint8_t rxBuffer[3];
volatile uint8_t rxCount = 0;
volatile uint8_t rxDone = 0;

void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        // Read one byte from the receive data register
        rxBuffer[rxCount++] = USART_ReceiveData(USART2);

        if(rxCount == 3) {
            rxDone = 1;
            // Disable the USART2 Receive Interrupt to stop receiving
            USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
        }
    }
}

int main(void) {
    AppInit();
    UART1_Init();
    UART2_Init();

    Log_Print("\r\n--- FULL UART TEST BOOTED ---\r\n");

    // Phase 1: TX
    Log_Print("Phase 1: TX\r\n");
    uint8_t txData[3] = {0xAA, 0xBB, 0xCC};
    for(uint8_t i = 0; i < 3; i++) {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, txData[i]);
    }
    // Wait for final transmission complete
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
    Log_Print("Phase 1: DONE\r\n");

    // Phase 2: RX
    Log_Print("Phase 2: RX\r\n");
    Log_Print("WAITING_FOR_UART_RX\r\n");

    while(!rxDone) {
        // Wait for interrupt to process data
    }

    Log_Print("Phase 2: DONE\r\n");

    // Verify
    if(rxBuffer[0] == 0x11 && rxBuffer[1] == 0x22 && rxBuffer[2] == 0x33) {
        Log_Print("Test Complete!\r\n");
        Log_Print("Verify: SUCCESS\r\n");
    } else {
        Log_Print("Test Complete!\r\n");
        Log_Print("Verify: FAILED\r\n");
    }

    while(1);
}
