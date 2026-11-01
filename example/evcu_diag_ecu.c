#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can.h"
#include "CanIf.h"
#include "Can_GeneralTypes.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

#include <string.h>

#define DIAG_REQ_CAN_ID  0x7E0u
#define DIAG_RESP_CAN_ID 0x7E8u

volatile uint8 diag_trigger_session = 0u;
volatile uint8 diag_trigger_vin = 0u;
volatile uint8 diag_trigger_snapshot = 0u;
volatile uint8 diag_trigger_dtc = 0u;
volatile uint8 diag_trigger_clear = 0u;
volatile uint8 diag_trigger_rpm = 0u;
volatile uint8 diag_trigger_temp = 0u;
volatile uint8 diag_last_response[64];
volatile uint16 diag_last_response_len = 0u;
volatile uint8 diag_last_complete = 0u;
volatile uint8 diag_last_sid = 0u;

static uint8 s_rx_expected_sn = 1u;
static uint16 s_rx_total = 0u;
static volatile uint8 s_fc_pending = 0u;
static volatile uint8 s_fc_delay_cycles = 0u;

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

static void can_send(Can_HwHandleType hth, uint16 can_id, const uint8 *payload, uint8 len)
{
    Can_PduType pdu;
    pdu.swPduHandle = 0u;
    pdu.id = can_id;
    pdu.length = len;
    pdu.sdu = (uint8*)payload;
    (void)Can_Write(hth, &pdu);
}

static void diag_send_sf(const uint8 *uds, uint8 len)
{
    uint8 frame[8] = {0};
    frame[0] = len & 0x0Fu;
    for (uint8 i = 0u; i < len; ++i) {
        frame[1u + i] = uds[i];
    }

    diag_last_response_len = 0u;
    diag_last_complete = 0u;
    diag_last_sid = 0u;
    can_send(3u, DIAG_REQ_CAN_ID, frame, 8u);
    Log_Print("[DIAG_ECU] TX UDS request\r\n");
}

static void diag_send_fc(void)
{
    uint8 fc[8] = {0x30u, 0x00u, 0x00u, 0u, 0u, 0u, 0u, 0u};
    can_send(4u, DIAG_REQ_CAN_ID, fc, 8u);
}

void App_CanRxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    if ((canId != DIAG_RESP_CAN_ID) || (PduInfoPtr == 0) ||
        (PduInfoPtr->SduDataPtr == 0) || (PduInfoPtr->SduLength == 0u)) {
        return;
    }

    const uint8 *data = PduInfoPtr->SduDataPtr;
    uint8 pci_type = (uint8)(data[0] >> 4u);

    if (pci_type == 0u) {
        uint8 len = data[0] & 0x0Fu;
        if (len > 63u) len = 63u;
        for (uint8 i = 0u; i < len; ++i) {
            diag_last_response[i] = data[1u + i];
        }
        diag_last_response_len = len;
        diag_last_sid = diag_last_response[0];
        diag_last_complete = 1u;
        Log_Print("[DIAG_ECU] RX SF response\r\n");
    } else if (pci_type == 1u) {
        s_rx_total = (uint16)(((uint16)(data[0] & 0x0Fu) << 8u) | data[1]);
        if (s_rx_total > 64u) s_rx_total = 64u;
        diag_last_response_len = 0u;
        for (uint8 i = 0u; (i < 6u) && (diag_last_response_len < s_rx_total); ++i) {
            diag_last_response[diag_last_response_len++] = data[2u + i];
        }
        s_rx_expected_sn = 1u;
        s_fc_pending = 1u;
        s_fc_delay_cycles = 5u;
        Log_Print("[DIAG_ECU] RX FF response\r\n");
    } else if (pci_type == 2u) {
        uint8 sn = data[0] & 0x0Fu;
        if (sn == (s_rx_expected_sn & 0x0Fu)) {
            for (uint8 i = 0u; (i < 7u) && (diag_last_response_len < s_rx_total); ++i) {
                diag_last_response[diag_last_response_len++] = data[1u + i];
            }
            s_rx_expected_sn++;
            if (diag_last_response_len >= s_rx_total) {
                diag_last_sid = diag_last_response[0];
                diag_last_complete = 1u;
                Log_Print("[DIAG_ECU] RX MF response complete\r\n");
            }
        }
    }
}

int main(void)
{
    Log_Init();
    Log_Print("\r\n--- Diagnostic ECU sample for eVCU ---\r\n");
    Can_Init();
    CanIf_Init();

    while (1) {
        if (diag_trigger_session != 0u) {
            uint8 req[] = {0x10u, 0x03u};
            diag_trigger_session = 0u;
            diag_send_sf(req, sizeof(req));
        }
        if (diag_trigger_vin != 0u) {
            uint8 req[] = {0x22u, 0xF1u, 0x90u};
            diag_trigger_vin = 0u;
            diag_send_sf(req, sizeof(req));
        }
        if (diag_trigger_snapshot != 0u) {
            uint8 req[] = {0x22u, 0xF0u, 0x01u};
            diag_trigger_snapshot = 0u;
            diag_send_sf(req, sizeof(req));
        }
        if (diag_trigger_dtc != 0u) {
            uint8 req[] = {0x19u, 0x02u, 0xFFu};
            diag_trigger_dtc = 0u;
            diag_send_sf(req, sizeof(req));
        }
        if (diag_trigger_clear != 0u) {
            uint8 req[] = {0x14u, 0xFFu, 0xFFu, 0xFFu};
            diag_trigger_clear = 0u;
            diag_send_sf(req, sizeof(req));
        }
        if (diag_trigger_rpm != 0u) {
            uint8 req[] = {0x22u, 0x01u, 0x0Cu};
            diag_trigger_rpm = 0u;
            diag_send_sf(req, sizeof(req));
        }
        if (diag_trigger_temp != 0u) {
            uint8 req[] = {0x22u, 0x01u, 0x05u};
            diag_trigger_temp = 0u;
            diag_send_sf(req, sizeof(req));
        }

        Can_MainFunction_Read();
        if ((s_fc_pending != 0u) && (s_fc_delay_cycles > 0u)) {
            s_fc_delay_cycles--;
        }
        if ((s_fc_pending != 0u) && (s_fc_delay_cycles == 0u)) {
            s_fc_pending = 0u;
            diag_send_fc();
            Log_Print("[DIAG_ECU] TX FC\r\n");
        }
        Can_MainFunction_Write();
        delay_ms(1u);
    }
}
