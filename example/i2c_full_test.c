#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_i2c.h"
#include "misc.h"

#define SLAVE_ADDRESS 0xA0

void Log_Print(const char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

static void Config_All(void) {
    // 1. USART1 Config
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

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

    // 2. I2C1 Config
    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_Init(I2C1, &I2C_InitStructure);
    I2C_Cmd(I2C1, ENABLE);
}

void I2C_WriteBytes(uint8_t address, uint8_t* data, uint8_t len) {
    // Wait for bus to be free
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY));

    // Send START condition
    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    // Send Slave Address + Write
    I2C_Send7bitAddress(I2C1, address, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    // Send data bytes
    for(uint8_t i = 0; i < len; i++) {
        I2C_SendData(I2C1, data[i]);
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    }

    // Send STOP condition
    I2C_GenerateSTOP(I2C1, ENABLE);
}

void I2C_ReadBytes(uint8_t address, uint8_t* buffer, uint8_t len) {
    // Wait for bus to be free
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY));

    // Send START condition
    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    // Send Slave Address + Read
    I2C_Send7bitAddress(I2C1, address, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // Wait for playright to inject data. We tell playwright we are waiting.
    Log_Print("WAITING_FOR_RX\r\n");

    for(uint8_t i = 0; i < len; i++) {
        if(i == len - 1) {
            // Disable ACK for last byte
            I2C_AcknowledgeConfig(I2C1, DISABLE);
            // Send STOP condition
            I2C_GenerateSTOP(I2C1, ENABLE);
        }
        
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED));
        buffer[i] = I2C_ReceiveData(I2C1);
    }
    
    // Re-enable ACK
    I2C_AcknowledgeConfig(I2C1, ENABLE);
}

int main(void) {
    Config_All();

    Log_Print("\r\n--- FULL I2C TEST BOOTED ---\r\n");

    // Phase 1: TX
    Log_Print("Phase 1: TX\r\n");
    uint8_t txData[3] = {0xAA, 0xBB, 0xCC};
    I2C_WriteBytes(SLAVE_ADDRESS, txData, 3);
    Log_Print("Phase 1: DONE\r\n");

    // Phase 2: RX
    Log_Print("Phase 2: RX\r\n");
    uint8_t rxData[3] = {0};
    I2C_ReadBytes(SLAVE_ADDRESS, rxData, 3);
    Log_Print("Phase 2: DONE\r\n");

    // Verification
    if (rxData[0] == 0x11 && rxData[1] == 0x22 && rxData[2] == 0x33) {
        Log_Print("Test Complete!\r\n");
        Log_Print("Verify: SUCCESS\r\n");
    } else {
        Log_Print("Test Failed: Data Mismatch\r\n");
    }

    while(1);
}
