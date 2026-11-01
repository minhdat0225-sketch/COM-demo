#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "LinIf.h"
#include "Lin.h"
#include <stdio.h>

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

void LED_Toggle(void) {}

static void BSW_Init_LIN_Only(void)
{
    /* Khởi tạo LIN Interface (bên trong gọi Lin_Init) */
    LinIf_Init();

    /* Khởi tạo PDU Router và COM
     * Lưu ý: Cần chắc chắn PduR_ConfigPB không bị vướng bận route của CAN */
    PduR_Init(&PduR_ConfigPB);
    Com_Init();
}

/* ===========================================================
 * Demo_LIN_LightCtrl – Gửi tín hiệu điều khiển đèn (LIN)
 * ===========================================================*/
static void Demo_LIN_LightCtrl(void)
{
    static boolean headlamp = TRUE;
    static boolean drl = FALSE;
    static uint8 brightness = 0u;

    /* Thay đổi data động */
    headlamp = (headlamp == TRUE) ? FALSE : TRUE;
    drl = (drl == TRUE) ? FALSE : TRUE;
    brightness = (brightness + 10u) % 101u;  /* Tăng từ 0 đến 100 rồi quay số lại */

    Com_SendSignal(ComSig_Light_Headlamp, &headlamp);
    Com_SendSignal(ComSig_Light_DRL, &drl);
    Com_SendSignal(ComSig_Light_Brightness, &brightness);
}

/* ===========================================================
 * Demo_LIN_HVACCtrl – Gửi tín hiệu điều khiển điều hòa (LIN)
 * ===========================================================*/
static void Demo_LIN_HVACCtrl(void)
{
    static uint8 fanSpeed = 10u;

    /* Thay đổi data động */
    fanSpeed = (fanSpeed + 15u) % 255u;  /* Tăng dần giới hạn 255 */

    Com_SendSignal(ComSig_HVAC_FanSpeed, &fanSpeed);
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Node TX (LIN Only Demo) Booted ---\r\n");

    BSW_Init_LIN_Only();

    uint8 temp_val = 0;
    uint8 temp_stat = 0;
    uint8 last_temp_val = 0xFF;

    while (1) {
        /* Cập nhật các Signal của khung LIN (vào COM buffer) */
        Demo_LIN_LightCtrl();
        Demo_LIN_HVACCtrl();

        /* Gọi LinIf để Schedule Table thực thi và PULL/RX data */
        LinIf_MainFunction();

        /* Đọc dữ liệu nhận về từ cảm biến nhiệt độ LIN */
        if (Com_ReceiveSignal(ComSig_Temp_Value, &temp_val) == E_OK) {
            if (temp_val != last_temp_val) {
                Com_ReceiveSignal(ComSig_Temp_Status, &temp_stat);
                char msg[100];
                sprintf(msg, "RX: Received LIN TempSensor -> Temp=%u, Status=%u\r\n", temp_val, temp_stat);
                Log_Print(msg);
                last_temp_val = temp_val;
            }
        }

        /* Delay mô phỏng chu kỳ giống định thời của AUTOSAR OS */
        delay_ms(100u);
    }
    return 0;
}
