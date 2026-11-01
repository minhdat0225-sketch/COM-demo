#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "CanIf.h"
#include "Can.h"
#include "../autosar/dcm/Dcm.h"
#include <stdio.h>
#include <string.h>

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

void BSW_Init_RX(void) {
    Can_Init();
    CanIf_Init();
    PduR_Init(&PduR_ConfigPB);
    Com_Init();
    Dcm_Init();  /* Khởi tạo module DCM */
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- VCU ECU (Node RX) Booted ---\r\n");

    BSW_Init_RX();

    uint16 rpm = 0;
    uint8 temp = 0;
    uint16 last_rpm = 0xFFFF;

    while (1) {
        /* 1. Polling dữ liệu từ Driver CAN (ISR cũng sẽ gọi lên CanIf_RxIndication) */
        Can_MainFunction_Read();

        /* 2. Đọc Signal từ COM buffer */
        if (Com_ReceiveSignal(ComSig_Engine_RPM, &rpm) == E_OK) {
            /* Chỉ in log nếu giá trị thay đổi để tránh tràn console */
            if (rpm != last_rpm) {
                Com_ReceiveSignal(ComSig_Engine_Temp, &temp);

                char msg[80];
                sprintf(msg, "RX: Received EngineStatus -> RPM=%u, Temp=%u\r\n", rpm, temp);
                Log_Print(msg);

                last_rpm = rpm;
            }
        }

        delay_ms(10u);
    }

    return 0;
}
