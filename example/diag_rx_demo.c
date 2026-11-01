#include "Std_Types.h"
#include "ComStack_Types.h"
#include "PduR_Cfg.h"
#include "CanTp.h"
#include "CanIf.h"
#include "Can.h"
#include "../autosar/dcm/Dcm.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

static void delay_ms(volatile uint32 ms)
{
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

void LED_Toggle(void) {
    /* Dummy function để thỏa mãn CanTp.c */
}

static void BSW_Init_RX(void)
{
    Can_Init();
    CanIf_Init();
    CanTp_Init();
    PduR_Init(&PduR_ConfigPB);
    Dcm_Init();   /* Tích hợp khởi động bộ nhận dữ liệu UDS */
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Node RX (DCM Target) Booted ---\r\n");
    Log_Print("Waiting for CanTp diagnostic packets on CAN bus...\r\n");

    BSW_Init_RX();

    while (1) {
        /* Duy trì các State Machine - Dữ liệu khi nhận xong sẽ được Dcm in Log tự động */
        Can_MainFunction_Read();
        Can_MainFunction_Write();
        CanTp_MainFunction();

        delay_ms(10u);
    }
    return 0;
}
