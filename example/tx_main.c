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

volatile uint8_t trigger_tx = 0;

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

static void BSW_Init(void)
{
    Can_Init();
    CanIf_Init();
    LinIf_Init();
    CanTp_Init();
    PduR_Init(&PduR_ConfigPB);
    Com_Init();
}

void LED_Toggle(void) {
    /* Dummy for CanTp.c */
}

static void Demo_CANTP_DiagTx(void)
{
    uint8 diagData[20] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
        0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05
    };
    
    PduInfoType tpInfo;
    tpInfo.SduDataPtr = diagData;
    tpInfo.SduLength  = 20;
    
    CanTp_Transmit(CanTpConf_CanTpTxNSdu_DiagTx, &tpInfo);
    Log_Print("[Node TX] Initiated CanTp Diagnostics transmission of 20 bytes!\r\n");
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Node TX Booted ---\r\n");
    Log_Print("Waiting for GDB to set trigger_tx = 1 ...\r\n");

    BSW_Init();

    /* Khởi tạo dữ liệu LIN vào Shadow Buffer của COM (Cơ chế Pull sẽ kéo nó ra định kỳ) */
    uint8 brightness = 80u;
    Com_SendSignal(ComSig_Light_Brightness, &brightness);
    uint8 fanSpeed = 128u;
    Com_SendSignal(ComSig_HVAC_FanSpeed, &fanSpeed);

    while (1) {
        if (trigger_tx == 1) {
            trigger_tx = 0; /* Reset trigger */
            Demo_CANTP_DiagTx();
            Log_Print("[Node TX] Transmission sent.\r\n");
        }

        CanTp_MainFunction();
        Can_MainFunction_Read();
        Can_MainFunction_Write();
        LinIf_MainFunction();

        delay_ms(10u);
    }
    return 0;
}
