#include "Std_Types.h"
#include "ComStack_Types.h"
#include "PduR_Cfg.h"
#include "CanTp.h"
#include "CanIf.h"
#include "Can.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

/* Biến cờ này sẽ được phần mềm Python / GDB ghi đè từ 0 -> 1 để kích hoạt gửi */
volatile uint8_t trigger_diag = 0;

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

static void BSW_Init_TX(void)
{
    Can_Init();
    CanIf_Init();
    CanTp_Init();       /* Quan trọng nhất cho việc chạy Diagnostic TP */
    PduR_Init(&PduR_ConfigPB);
}

static void Broadcast_CanTp_UDS(void)
{
    /* Mảng data 20 byte giả lập gói UDS (chẳng hạn 0x22 - ReadDataByIdentifier) */
    uint8 udsData[20] = {
        0x22, 0xF1, 0x90, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02
    };
    
    PduInfoType tpInfo;
    tpInfo.SduDataPtr = udsData;
    tpInfo.SduLength  = 20;
    
    /* Gửi lên Layer CanTp thông qua ID cấu hình sẵn txNSdu_DiagTx (VD ID=0) */
    CanTp_Transmit(0, &tpInfo);
    Log_Print("[Node TX] Da gui tin hieu CanTp Diagnostic UDS (20 bytes) vao mang!\r\n");
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Node TX (Diagnostic Emitter) Booted ---\r\n");
    Log_Print("Waiting for Python Tool to set trigger_diag = 1 ...\r\n");

    BSW_Init_TX();

    while (1) {
        static uint32 tick_count = 0;
        tick_count++;
        /* Chờ tín hiệu từ GDB Tool Test hoặc tự động gửi mỗi 2 giây (200 ticks) */
        if (trigger_diag == 1 || tick_count >= 200) {
            trigger_diag = 0;
            tick_count = 0;
            Broadcast_CanTp_UDS();
        }

        /* Phải duy trì vòng lặp các module thì state machine mới hoạt động */
        CanTp_MainFunction();
        Can_MainFunction_Read();
        Can_MainFunction_Write();

        delay_ms(10u);
    }
    return 0;
}
