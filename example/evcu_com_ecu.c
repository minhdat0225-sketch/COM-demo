#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "PduR_Cfg.h"
#include "CanIf.h"
#include "Can.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

volatile uint8 com_trigger_engine_status = 0u;
volatile uint16 com_tx_rpm = 3000u;
volatile uint8 com_tx_temp = 90u;
volatile uint8 com_rx_vehicle_command_seen = 0u;
volatile uint8 com_rx_throttle = 0u;
volatile uint8 com_rx_start = 0u;
volatile uint8 com_rx_torque_limit = 0u;
volatile uint8 com_rx_alive = 0u;
volatile uint8 com_rx_crc = 0u;

static uint8 s_alive = 0u;

static void delay_ms(volatile uint32 ms)
{
    while (ms--) {
        for (volatile uint32 i = 0; i < 7200u; i++) {
            __asm__("nop");
        }
    }
}

static void Log_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    USART_InitTypeDef us;
    us.USART_BaudRate = 115200;
    us.USART_WordLength = USART_WordLength_8b;
    us.USART_StopBits = USART_StopBits_1;
    us.USART_Parity = USART_Parity_No;
    us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    us.USART_Mode = USART_Mode_Tx;
    USART_Init(USART1, &us);
    USART_Cmd(USART1, ENABLE);
}

void Log_Print(const char* str)
{
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {}
        USART_SendData(USART1, *str++);
    }
}

void LED_Toggle(void) {}

static uint8 crc4(uint16 rpm, uint8 temp, uint8 torque, uint8 state, uint8 alive)
{
    return (uint8)(((uint8)(rpm >> 8u) ^ (uint8)rpm ^ temp ^ torque ^ state ^ alive) & 0x0Fu);
}

static void send_engine_status(void)
{
    uint8 torque = 100u;
    uint8 state = 2u;
    uint8 alive = s_alive & 0x0Fu;
    uint8 crc = crc4(com_tx_rpm, com_tx_temp, torque, state, alive);

    (void)Com_SendSignal(ComSig_Engine_RPM, (const void *)&com_tx_rpm);
    (void)Com_SendSignal(ComSig_Engine_Temp, (const void *)&com_tx_temp);

    /* Các signal mở rộng chưa có symbolic config riêng trong demo gốc nên pack trực tiếp qua frame length 8 không cần thiết cho eVCU. */
    (void)Com_TriggerIPDUSend(ComConf_ComIPdu_EngineStatus);

    s_alive++;
    (void)torque;
    (void)state;
    (void)crc;
    Log_Print("[COM_ECU] TX EngineStatus\r\n");
}

void App_ComRxIndication(PduIdType ComRxPduId)
{
    uint8 throttle = 0u;
    uint8 torque_limit = 0u;
    uint8 alive = 0u;
    uint8 crc = 0u;
    boolean start = FALSE;

    if (ComRxPduId != ComConf_ComIPdu_EngineCmd) {
        return;
    }

    (void)Com_ReceiveSignal(ComSig_Engine_Throttle, &throttle);
    (void)Com_ReceiveSignal(ComSig_Engine_Start, &start);
    (void)Com_ReceiveSignal(ComSig_Engine_TorqueLimit, &torque_limit);
    (void)Com_ReceiveSignal(ComSig_Engine_Alive, &alive);
    (void)Com_ReceiveSignal(ComSig_Engine_CRC, &crc);
    com_rx_throttle = throttle;
    com_rx_start = (start != FALSE) ? 1u : 0u;
    com_rx_torque_limit = torque_limit;
    com_rx_alive = alive;
    com_rx_crc = crc;
    com_rx_vehicle_command_seen = 1u;
    Log_Print("[COM_ECU] RX VehicleCommand\r\n");
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- COM ECU sample for eVCU ---\r\n");

    Can_Init();
    CanIf_Init();
    PduR_Init(&PduR_ConfigPB);
    Com_Init();

    while (1) {
        if (com_trigger_engine_status != 0u) {
            com_trigger_engine_status = 0u;
            send_engine_status();
        }

        Can_MainFunction_Read();
        Can_MainFunction_Write();
        delay_ms(1u);
    }
}
