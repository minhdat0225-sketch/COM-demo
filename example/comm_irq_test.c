#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_can.h"
#include "misc.h"

volatile uint32_t UsartIrqCount = 0;
volatile uint32_t SpiIrqCount = 0;
volatile uint32_t I2cIrqCount = 0;
volatile uint32_t CanIrqCount = 0;

void Log_Print(const char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET) {
        UsartIrqCount++;
        USART_ITConfig(USART1, USART_IT_TXE, DISABLE); // Disable to avoid infinite loop
        Log_Print("USART1 TXE IRQ Triggered!\r\n");
    }
}

void SPI1_IRQHandler(void) {
    if (SPI_I2S_GetITStatus(SPI1, SPI_I2S_IT_TXE) != RESET) {
        SpiIrqCount++;
        SPI_I2S_ITConfig(SPI1, SPI_I2S_IT_TXE, DISABLE); // Disable to avoid infinite loop
        Log_Print("SPI1 TXE IRQ Triggered!\r\n");
    }
}

void I2C1_EV_IRQHandler(void) {
    if (I2C_GetITStatus(I2C1, I2C_IT_SB) != RESET) {
        I2cIrqCount++;
        I2C_ITConfig(I2C1, I2C_IT_EVT, DISABLE); // Disable
        Log_Print("I2C1 SB IRQ Triggered!\r\n");
    }
}

void USB_HP_CAN1_TX_IRQHandler(void) {
    if (CAN_GetITStatus(CAN1, CAN_IT_RQCP0)) {
        CAN_ClearITPendingBit(CAN1, CAN_IT_RQCP0);
        CanIrqCount++;
        CAN_ITConfig(CAN1, CAN_IT_TME, DISABLE);
        Log_Print("CAN1 TX IRQ Triggered!\r\n");
    }
}

static void Config_All(void) {
    // 1. USART1 Config (TXE)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1 | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 | RCC_APB1Periph_CAN1, ENABLE);

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

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 2. SPI1 Config (TXE)
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = SPI1_IRQn;
    NVIC_Init(&NVIC_InitStructure);

    // 3. I2C1 Config (EVT)
    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_Init(I2C1, &I2C_InitStructure);
    I2C_Cmd(I2C1, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = I2C1_EV_IRQn;
    NVIC_Init(&NVIC_InitStructure);

    // 4. CAN1 Config (TX)
    CAN_InitTypeDef CAN_InitStructure;
    CAN_StructInit(&CAN_InitStructure);
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = DISABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_8tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_3tq;
    CAN_InitStructure.CAN_Prescaler = 6;
    CAN_Init(CAN1, &CAN_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USB_HP_CAN1_TX_IRQn;
    NVIC_Init(&NVIC_InitStructure);
}

int main(void) {
    Config_All();

    Log_Print("\r\n--- COMM IRQ FULL TEST BOOTED ---\r\n");

    // Enable USART TXE Interrupt -> Should trigger immediately
    USART_ITConfig(USART1, USART_IT_TXE, ENABLE);

    // Enable SPI TXE Interrupt -> Should trigger immediately
    SPI_I2S_ITConfig(SPI1, SPI_I2S_IT_TXE, ENABLE);

    // Enable I2C EVT Interrupt and send START -> Should trigger SB
    I2C_ITConfig(I2C1, I2C_IT_EVT, ENABLE);
    I2C_GenerateSTART(I2C1, ENABLE);

    // Enable CAN TX Interrupt and transmit a message
    CAN_ITConfig(CAN1, CAN_IT_TME, ENABLE);
    CanTxMsg TxMessage;
    TxMessage.StdId = 0x123;
    TxMessage.IDE = CAN_Id_Standard;
    TxMessage.RTR = CAN_RTR_Data;
    TxMessage.DLC = 1;
    TxMessage.Data[0] = 0xAA;
    CAN_Transmit(CAN1, &TxMessage);

    // Wait for all 4 interrupts to fire
    while(UsartIrqCount < 1 || SpiIrqCount < 1 || I2cIrqCount < 1 || CanIrqCount < 1) {
        // Wait
    }

    Log_Print("Test Complete!\r\n");
    Log_Print("Verify: SUCCESS\r\n");

    while(1);
}
