#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

// Vòng lặp trễ đơn giản (hiệu năng cao trong giả lập)
void delay_sim(volatile uint32_t count) {
    while(count--) {
        __asm("nop");
    }
}

int main(void) {
    // 1. Cấp xung nhịp cho Port A, B, C
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    // 2. Cấu hình Port B (Output Push-Pull) - Dùng để test Output
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All; // Tất cả các chân PB0-PB15
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. Cấu hình Port A (Input Pull-Up) - Dùng để test Input
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All; // Tất cả các chân PA0-PA15
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Vòng lặp chính
    while (1) {
        // --- PHASE 1: TEST OUTPUT ---
        // Bật tất cả Port B lên HIGH
        GPIO_SetBits(GPIOB, GPIO_Pin_All);
        delay_sim(10000); // Chờ ngắn
        
        // Tắt tất cả Port B xuống LOW
        GPIO_ResetBits(GPIOB, GPIO_Pin_All);
        delay_sim(10000);

        // --- PHASE 2: TEST INPUT ---
        // Đọc giá trị từ Port A (Input) và xuất thẳng ra Port B (Output)
        uint16_t input_val = GPIO_ReadInputData(GPIOA);
        GPIO_Write(GPIOB, input_val);
        delay_sim(10000);
    }
}

void Log_Print(const char* format, ...) {}

