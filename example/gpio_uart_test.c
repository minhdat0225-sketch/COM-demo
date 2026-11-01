#include <stdio.h>
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

static void delay_ms(volatile uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 7200u; i++) {
            __asm__("nop");
        }
    }
}

static void Log_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    
    // Khởi tạo UART TX (PA9)
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Khởi tạo UART
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);

    // Khởi tạo Nút nhấn (PA0) làm ngõ vào
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // Input Pull-Up
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void Log_Print(const char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

int main(void) {
    Log_Init();
    Log_Print("\r\n--- GPIO & UART Test Booted ---\r\n");
    Log_Print("Waiting for Button PA0 to be pressed...\r\n");

    uint8_t last_state = 1; // Giả sử Pull-up (thường mức 1, nhấn mức 0), hoặc tùy mạch

    while (1) {
        uint8_t current_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
        
        if (current_state != last_state) {
            delay_ms(50); // Debounce
            current_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0); // Đọc lại
            
            if (current_state != last_state) {
                last_state = current_state;
                
                if (current_state == 1) {
                    Log_Print("[UART] Button PA0 is HIGH (Released / Default)\r\n");
                } else {
                    Log_Print("[UART] Button PA0 is LOW (Pressed)\r\n");
                }
            }
        }
        
        delay_ms(10);
    }
    return 0;
}
