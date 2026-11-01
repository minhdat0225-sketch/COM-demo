#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "CanIf.h"
#include "Can.h"
#include "LinIf.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

/* Extern these from CanTp.c */
extern volatile uint32 CanTp_RxDoneCount;
extern uint8_t *Debug_Get_CanTp_RxBuffer(void);

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

static void Log_HexPrint(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, hex[(val >> 4) & 0x0F]);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, hex[val & 0x0F]);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, ' ');
}

static void BSW_Init(void)
{
    Can_Init();
    CanIf_Init();
    LinIf_Init();
    CanTp_Init();
    PduR_Init(&PduR_ConfigPB);
    Com_Init();
}

/* Biến dummy để thỏa mãn LED_Toggle trong CanTp.c */
void LED_Toggle(void) {
    /* Do nothing */
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Node RX Booted ---\r\n");
    Log_Print("Waiting for CanTp diagnostic packets on CAN bus...\r\n");

    BSW_Init();

    uint32 last_rx_count = 0;

    while (1) {
        if (CanTp_RxDoneCount != last_rx_count) {
            last_rx_count = CanTp_RxDoneCount;
            Log_Print("\r\n[Node RX] Received CanTp Payload (20 bytes): ");
            
            uint8_t *buf = Debug_Get_CanTp_RxBuffer();
            for (int i = 0; i < 20; i++) {
                Log_HexPrint(buf[i]);
            }
            Log_Print("\r\n");
        }

        CanTp_MainFunction();
        Can_MainFunction_Read();
        Can_MainFunction_Write();
        LinIf_MainFunction();

        delay_ms(10u);
    }
    return 0;
}
