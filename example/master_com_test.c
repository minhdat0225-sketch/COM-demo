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

// Variables to simulate changing data
static uint8 throttle_val = 0;
static uint8 brake_val = 0;
static uint8 regen_val = 0;
static boolean headlamp_st = FALSE;
static boolean turnL_st = FALSE;
static boolean turnR_st = FALSE;

// Cycle counters
static uint32 tick_10ms = 0;
static uint32 tick_20ms = 0;
static uint32 tick_100ms = 0;

volatile uint32 debug_counter = 0;

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

static void task_10ms(void)
{
    // Log_Print("10\r\n");
    // EngineCmd (0x180) - throttle changes quickly
    throttle_val++;
    if (throttle_val > 100) throttle_val = 0;
    
    boolean start_st = TRUE;
    uint8 torque_limit = 200;
    static uint8 alive_cnt = 0;
    alive_cnt = (alive_cnt + 1) & 0x0F;
    uint8 crc = 0x0A; // Mock CRC

    (void)Com_SendSignal(ComSig_Engine_Throttle, &throttle_val);
    (void)Com_SendSignal(ComSig_Engine_Start, &start_st);
    (void)Com_SendSignal(ComSig_Engine_TorqueLimit, &torque_limit);
    (void)Com_SendSignal(ComSig_Engine_Alive, &alive_cnt);
    (void)Com_SendSignal(ComSig_Engine_CRC, &crc);

    (void)Com_TriggerIPDUSend(ComConf_ComIPdu_EngineCmd);

    uint8_t debug_data[4];
    debug_data[0] = debug_counter & 0xFF;
    debug_data[1] = (debug_counter >> 8) & 0xFF;
    debug_data[2] = 0;
    debug_data[3] = 0;
    Can_PduType debug_pdu = { .id = 0x123, .length = 4, .sdu = debug_data };
    Can_Write(0, &debug_pdu);
}

static void task_20ms(void)
{
    debug_counter++;
    // Log_Print("20\r\n");
    // BrakeCmd (0x280)
    brake_val += 2;
    if (brake_val > 100) brake_val = 0;
    
    regen_val = 100 - brake_val;
    static uint8 alive_cnt = 0;
    alive_cnt = (alive_cnt + 1) & 0x0F;
    uint8 crc = 0x0B;

    (void)Com_SendSignal(ComSig_Brake_BrakeReq, &brake_val);
    (void)Com_SendSignal(ComSig_Brake_RegenReq, &regen_val);
    (void)Com_SendSignal(ComSig_Brake_Alive, &alive_cnt);
    (void)Com_SendSignal(ComSig_Brake_CRC, &crc);

    (void)Com_TriggerIPDUSend(ComConf_ComIPdu_BrakeCmd);

    // DEBUG: Force CAN write to 0x280 to see if task_20ms runs
    uint8_t debug_data2[3] = {0xDD, 0xEE, 0xFF};
    Can_PduType debug_pdu2 = { .id = 0x280, .length = 3, .sdu = debug_data2 };
    Can_Write(0, &debug_pdu2);
}

static void task_100ms(void)
{
    // Log_Print("100\r\n");
    // BodyCmd (0x380) - blinkers and headlamp
    static uint8 toggle = 0;
    toggle++;
    
    headlamp_st = (toggle % 4 == 0) ? TRUE : FALSE;
    turnL_st = (toggle % 2 == 0) ? TRUE : FALSE;
    turnR_st = !turnL_st;
    boolean door_lock = TRUE;

    (void)Com_SendSignal(ComSig_Body_Headlamp, &headlamp_st);
    (void)Com_SendSignal(ComSig_Body_TurnL, &turnL_st);
    (void)Com_SendSignal(ComSig_Body_TurnR, &turnR_st);
    (void)Com_SendSignal(ComSig_Body_DoorLock, &door_lock);

    (void)Com_TriggerIPDUSend(ComConf_ComIPdu_BodyCmd);
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Master EVCU Multi-COM Test ---\r\n");

    Can_Init();
    CanIf_Init();
    PduR_Init(&PduR_ConfigPB);
    Com_Init();

    while (1) {
        task_10ms();
        task_20ms();
        task_100ms();
        
        Can_MainFunction_Read();
        Can_MainFunction_Write();
        
        // 1ms tick
        delay_ms(1u);
    }
}
