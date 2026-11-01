#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "misc.h"

static void delay_ms(volatile uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 100u; i++) {
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

void Log_PrintInt(uint32_t val) {
    char buf[16];
    int i = 0;
    if (val == 0) {
        Log_Print("0");
        return;
    }
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i > 0) {
        char c[2] = {buf[--i], 0};
        Log_Print(c);
    }
}

volatile uint16_t adc_irq_val = 0;
volatile uint8_t adc_irq_flag = 0;

void ADC1_2_IRQHandler(void) {
    if (ADC_GetITStatus(ADC1, ADC_IT_EOC)) {
        adc_irq_val = ADC_GetConversionValue(ADC1); // Reading DR clears EOC
        adc_irq_flag = 1;
        ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
    }
}

int main(void) {
    Log_Init();
    Log_Print("\r\n--- ADC FULL TEST BOOTED ---\r\n");

    // Enable ADC1 Clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);

    // Configure PA0 (Channel 0) and PA1 (Channel 1) as Analog Input
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ADC Configuration
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; // Single Mode
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // Software Trigger
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // Enable ADC1
    ADC_Cmd(ADC1, ENABLE);

    // Configure NVIC for ADC Interrupt (Test 3)
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = ADC1_2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    while (1) {
        Log_Print("\r\n[Test 1] Polling Channel 0...\r\n");
        ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        uint16_t val0 = ADC_GetConversionValue(ADC1);
        Log_Print("ADC CH0: ");
        Log_PrintInt(val0);
        Log_Print("\r\n");
        
        delay_ms(100);

        Log_Print("[Test 2] Polling Channel 1...\r\n");
        ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        uint16_t val1 = ADC_GetConversionValue(ADC1);
        Log_Print("ADC CH1: ");
        Log_PrintInt(val1);
        Log_Print("\r\n");

        delay_ms(100);

        Log_Print("[Test 3] Interrupt Channel 0...\r\n");
        ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
        adc_irq_flag = 0;
        ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE); // Bật ngắt
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        
        while(adc_irq_flag == 0); // Đợi ngắt nổ
        
        ADC_ITConfig(ADC1, ADC_IT_EOC, DISABLE); // Tắt ngắt
        Log_Print("ADC IRQ CH0: ");
        Log_PrintInt(adc_irq_val);
        Log_Print("\r\n");

        delay_ms(500);
    }
}
