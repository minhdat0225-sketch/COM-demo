#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "CanIf.h"
#include "Can.h"
#include <stdio.h>

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

static void delay_ms(volatile uint32 ms) {
    while (ms--) {
        for (volatile uint32 i = 0; i < 7200u; i++) {
            __asm__("nop");
        }
    }
}

static void Log_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

void Log_Print(const char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

void BSW_Init_TX(void) {
    Can_Init();
    CanIf_Init();
    PduR_Init(&PduR_ConfigPB);
    Com_Init();
}

int main(void) {
    Log_Init();
    Log_Print("\r\n--- Engine ECU (Node TX) Booted ---\r\n");

    BSW_Init_TX();

    uint16 rpm = 1000u;
    uint8 temp = 70u;

    while (1) {
        rpm += 50u;
        if (rpm > 8000u) rpm = 800u;
        
        temp += 1u;
        if (temp > 110u) temp = 70u;

        /* Cập nhật Signal vào COM shadow buffer */
        Com_SendSignal(ComSig_Engine_RPM, &rpm);
        Com_SendSignal(ComSig_Engine_Temp, &temp);

        char msg[64];
        sprintf(msg, "TX: Sending EngineStatus -> RPM=%u, Temp=%u\r\n", rpm, temp);
        Log_Print(msg);

        /* Kích phát gửi PDU xuống tầng dưới */
        Com_TriggerIPDUSend(ComConf_ComIPdu_EngineStatus);

        /* Duy trì polling cho TX confirmation (giả lập Task) */
        Can_MainFunction_Write();

        delay_ms(500u);
    }
    return 0;
}
