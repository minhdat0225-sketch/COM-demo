#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include <stdio.h>

void AppInit(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
}

void UART1_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // TX = PA9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

void Log_Print(const char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

void GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    // Button = PA0 (Input Pull-Down since UI button drives HIGH when pressed)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // LED = PA1 (Output Push-Pull)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // Default LED OFF
    GPIO_ResetBits(GPIOA, GPIO_Pin_1);
}

int main(void) {
    AppInit();
    UART1_Init();
    GPIO_Config();

    Log_Print("\n--- GPIO BUTTON TEST BOOTED ---\n");

    uint8_t last_state = 0;
    volatile uint32_t delay_cnt;
    volatile uint32_t tick_cnt = 0;
    
    while(1) {
        tick_cnt++;
        if (tick_cnt >= 10000) {
            Log_Print(".");
            tick_cnt = 0;
        }

        uint8_t current_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
        
        if (current_state != last_state) {
            if (current_state == Bit_SET) {
                Log_Print("RAW_STATE_HIGH\n");
            } else {
                Log_Print("RAW_STATE_LOW\n");
            }
            // Simple debounce (simulated environment doesn't strictly need this, but good practice)
            for(delay_cnt = 0; delay_cnt < 1000; delay_cnt++) {
                __asm("nop");
            }
            
            // Confirm state
            if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == current_state) {
                last_state = current_state;
                if (current_state == Bit_SET) {
                    GPIO_SetBits(GPIOA, GPIO_Pin_1);
                    Log_Print("BUTTON_PRESSED\n");
                } else {
                    GPIO_ResetBits(GPIOA, GPIO_Pin_1);
                    Log_Print("BUTTON_RELEASED\n");
                }
            } else {
                Log_Print("DEBOUNCE_FAILED\n");
            }
        }
    }
}
