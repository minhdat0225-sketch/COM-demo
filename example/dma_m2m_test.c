#include "stm32f10x_rcc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "misc.h"

#define BUFFER_SIZE 32

uint32_t SrcBuffer[BUFFER_SIZE];
uint32_t DstBuffer[BUFFER_SIZE];
volatile uint8_t TransferComplete = 0;

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

void Log_PrintInt(uint32_t val) {
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0) {
            buf[i--] = (val % 10) + '0';
            val /= 10;
        }
    }
    Log_Print(&buf[i + 1]);
}

void DMA1_Channel1_IRQHandler(void) {
    Log_Print("IRQ Entered!\r\n");
    if(DMA_GetITStatus(DMA1_IT_TC1)) {
        TransferComplete = 1;
        DMA_ClearITPendingBit(DMA1_IT_GL1);
    }
}

static void DMA_Config(void) {
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA1_Channel1);
    
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)SrcBuffer;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)DstBuffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;
    
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

int main(void) {
    Log_Init();
    Log_Print("\r\n--- DMA M2M FULL TEST BOOTED ---\r\n");

    for(uint32_t i = 0; i < BUFFER_SIZE; i++) {
        SrcBuffer[i] = 0xAABBCC00 + i;
        DstBuffer[i] = 0;
    }

    DMA_Config();
    Log_Print("Starting DMA M2M Transfer...\r\n");
    
    DMA_Cmd(DMA1_Channel1, ENABLE);

    while(TransferComplete == 0) {
        // Wait for interrupt
    }

    Log_Print("DMA Transfer Complete!\r\n");

    uint8_t success = 1;
    for(uint32_t i = 0; i < BUFFER_SIZE; i++) {
        if(DstBuffer[i] != SrcBuffer[i]) {
            success = 0;
            break;
        }
    }

    if(success) {
        Log_Print("Verify: SUCCESS\r\n");
    } else {
        Log_Print("Verify: FAIL\r\n");
    }
    
    Log_Print("DstBuffer[0] = ");
    Log_PrintInt(DstBuffer[0]);
    Log_Print("\r\nDstBuffer[31] = ");
    Log_PrintInt(DstBuffer[31]);
    Log_Print("\r\n");

    while(1);
}
