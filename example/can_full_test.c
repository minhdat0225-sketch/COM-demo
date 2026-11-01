#include "stm32f10x.h"
#include "stm32f10x_can.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "misc.h"
#include <stdio.h>

volatile uint8_t canRxDone = 0;
volatile uint32_t receivedId = 0;
volatile uint8_t receivedData[8] = {0};
volatile uint8_t receivedDlc = 0;

void AppInit(void) {
    // Basic system init
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
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

void CAN_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // Configure CAN pins (RX = PA11, TX = PA12)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // CAN initialization
    CAN_DeInit(CAN1);
    CAN_StructInit(&CAN_InitStructure);

    // 500Kbps at 36MHz APB1 clock
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = DISABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_12tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_5tq;
    CAN_InitStructure.CAN_Prescaler = 4;
    CAN_Init(CAN1, &CAN_InitStructure);

    // Filter configuration (accept all)
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStructure);

    // Enable FIFO 0 message pending Interrupt
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

    // NVIC Configuration
    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void USB_LP_CAN1_RX0_IRQHandler(void) {
    if (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET) {
        CanRxMsg RxMessage;
        CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);

        receivedId = (RxMessage.IDE == CAN_Id_Standard) ? RxMessage.StdId : RxMessage.ExtId;
        receivedDlc = RxMessage.DLC;
        for(int i = 0; i < receivedDlc; i++) {
            receivedData[i] = RxMessage.Data[i];
        }

        canRxDone = 1;
        
        CAN_ClearITPendingBit(CAN1, CAN_IT_FMP0);
    }
}

int main(void) {
    AppInit();
    UART1_Init();
    CAN_Config();

    Log_Print("\n--- FULL CAN TEST BOOTED ---\n");

    // ==========================================
    // Phase 1: TX
    // ==========================================
    Log_Print("Phase 1: TX\n");
    CanTxMsg TxMessage;
    TxMessage.StdId = 0x123;
    TxMessage.IDE = CAN_Id_Standard;
    TxMessage.RTR = CAN_RTR_Data;
    TxMessage.DLC = 4;
    TxMessage.Data[0] = 0xAA;
    TxMessage.Data[1] = 0xBB;
    TxMessage.Data[2] = 0xCC;
    TxMessage.Data[3] = 0xDD;

    uint8_t mailbox = CAN_Transmit(CAN1, &TxMessage);
    
    // Wait until transmission is ok
    int timeout = 0xFFFF;
    while(CAN_TransmitStatus(CAN1, mailbox) != CANTXOK && timeout > 0) {
        timeout--;
    }

    if (timeout > 0) {
        Log_Print("Phase 1: DONE\n");
    } else {
        Log_Print("Phase 1: FAILED\n");
    }

    // ==========================================
    // Phase 2: RX
    // ==========================================
    Log_Print("Phase 2: RX\n");
    Log_Print("WAITING_FOR_CAN_RX\n");

    while(!canRxDone) {
        // Wait for interrupt
    }

    Log_Print("Phase 2: DONE\n");

    // ==========================================
    // Verification
    // ==========================================
    if (receivedId == 0x7DF && receivedDlc == 4 && 
        receivedData[0] == 0x11 && receivedData[1] == 0x22 && 
        receivedData[2] == 0x33 && receivedData[3] == 0x44) {
        Log_Print("Verify: SUCCESS\n");
    } else {
        char buf[64];
        sprintf(buf, "Verify: FAILED (ID=0x%lX, DLC=%d)\n", (unsigned long)receivedId, receivedDlc);
        Log_Print(buf);
    }

    while(1);
}
