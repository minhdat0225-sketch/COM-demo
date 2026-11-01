/**********************************************************
 * @file    main.c
 * @brief   AUTOSAR Classic COM Demo – STM32F103
 *
 * @details Ứng dụng demo minh họa toàn bộ luồng truyền tín hiệu
 *          trong kiến trúc AUTOSAR Classic Communication Stack.
 *
 *          ╔════════════════════════════════════════════════════╗
 *          ║         KIẾN TRÚC AUTOSAR COM STACK               ║
 *          ╠════════════════════════════════════════════════════╣
 *          ║                                                    ║
 *          ║  ┌──────────────────────────┐                      ║
 *          ║  │   main() – SWC (App)     │  ← Tầng Ứng Dụng   ║
 *          ║  │   Com_SendSignal()       │                      ║
 *          ║  │   Com_TriggerIPDUSend()  │                      ║
 *          ║  └────────────┬─────────────┘                      ║
 *          ║               │                                    ║
 *          ║  ┌────────────▼─────────────┐                      ║
 *          ║  │         COM              │  ← Tầng Service     ║
 *          ║  │  Pack signal → I-PDU     │                      ║
 *          ║  │  PduR_ComTransmit()      │                      ║
 *          ║  └────────────┬─────────────┘                      ║
 *          ║               │                                    ║
 *          ║  ┌────────────▼─────────────┐                      ║
 *          ║  │        PduR              │  ← PDU Router       ║
 *          ║  │  Routing Table:          │                      ║
 *          ║  │  ┌────────────────────┐  │                      ║
 *          ║  │  │EngineCmd → CanIf   │  │                      ║
 *          ║  │  │BrakeCmd  → CanIf   │  │                      ║
 *          ║  │  │BodyCmd   → CanIf   │  │                      ║
 *          ║  │  │LightCtrl → LinIf   │  │                      ║
 *          ║  │  │HVACCtrl  → LinIf   │  │                      ║
 *          ║  │  └────────────────────┘  │                      ║
 *          ║  └───────┬──────────┬───────┘                      ║
 *          ║          │          │                               ║
 *          ║  ┌───────▼────┐ ┌──▼─────────┐                    ║
 *          ║  │   CanIf    │ │   LinIf     │ ← ECU Abstraction ║
 *          ║  │Can_Write() │ │Lin_SendFrame│                    ║
 *          ║  └──────┬─────┘ └──────┬──────┘                    ║
 *          ║         │              │                           ║
 *          ║  ┌──────▼─────┐ ┌──────▼──────┐                   ║
 *          ║  │CAN Driver  │ │ LIN Driver  │ ← MCAL            ║
 *          ║  │bxCAN (CAN1)│ │ USART2      │                   ║
 *          ║  │PA11/PA12   │ │ PA2/PA3     │                   ║
 *          ║  └────────────┘ └─────────────┘                   ║
 *          ╚════════════════════════════════════════════════════╝
 *
 *          Demo gửi 5 loại I-PDU:
 *            CAN: EngineCmd (0x180), BrakeCmd (0x280), BodyCmd (0x380)
 *            LIN (Time-Triggered / PULL): LightCtrl (ID 0x10), HVACCtrl (ID 0x11)
 *
 * @version 2.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/

/* ===== AUTOSAR BSW Includes ===== */
#include "Std_Types.h"         /* Kiểu dữ liệu chuẩn AUTOSAR        */
#include "ComStack_Types.h"    /* PduIdType, PduInfoType              */
#include "Com.h"               /* COM: Com_SendSignal, Com_TriggerIPDUSend */
#include "Com_Cfg.h"           /* Cấu hình COM: Signal ID, I-PDU ID  */
#include "PduR.h"              /* PDU Router: PduR_Init               */
#include "PduR_Cfg.h"          /* Cấu hình PduR: bảng route           */
#include "CanTp.h"             /* CAN Transport Protocol               */
#include "CanTp_Cfg.h"         /* Cấu hình CanTp                       */
#include "CanIf.h"             /* CAN Interface: CanIf_Init            */
#include "Can.h"               /* CAN Driver: Can_Init                 */
#include "LinIf.h"             /* LIN Interface: LinIf_Init            */

/* ===== SPL Includes ===== */
#include "stm32f10x_rcc.h"     /* SPL: Quản lý clock                  */
#include "stm32f10x_gpio.h"    /* SPL: Cấu hình GPIO                  */
#include "stm32f10x_usart.h"   /* SPL: Cấu hình USART                 */

/* ===========================================================
 * Log_Init / Log_Print – In debug log qua USART1
 * -----------------------------------------------------------
 * Cấu hình USART1 trên PA9 (TX) để in log ra console.
 * ===========================================================*/
static void Log_Init(void)
{
    /* Bật clock cho USART1 và GPIOA */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* Cấu hình TX (PA9): Alternate Function Push-Pull */
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* Cấu hình USART1: 115200 baud, 8-bit, 1 stop bit, no parity */
    USART_InitTypeDef usart;
    usart.USART_BaudRate            = 115200;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx;
    USART_Init(USART1, &usart);

    /* Bật USART1 */
    USART_Cmd(USART1, ENABLE);
}

void Log_Print(const char* str)
{
    while (*str) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

/* ===========================================================
 * delay_ms – Delay đơn giản (blocking)
 * -----------------------------------------------------------
 * Delay bằng vòng lặp NOP, ước tính dựa trên tần số CPU
 * 72MHz. Không chính xác tuyệt đối, chỉ dùng cho demo.
 *
 * 7200 vòng NOP ≈ 1ms tại 72MHz (mỗi NOP ≈ 1 cycle)
 * ===========================================================*/
static void delay_ms(volatile uint32 ms)
{
    while (ms--) {
        for (volatile uint32 i = 0; i < 7200u; i++) {
            __asm__("nop");
        }
    }
}

/* ===========================================================
 * LED_Init / LED_Toggle – Điều khiển LED trên BluePill
 * -----------------------------------------------------------
 * BluePill board có LED tại PC13 (active low):
 *   - Khi PC13 = 0 (LOW)  → LED sáng
 *   - Khi PC13 = 1 (HIGH) → LED tắt
 *
 * LED toggle mỗi vòng lặp main → xác nhận chương trình
 * đang chạy bình thường.
 * ===========================================================*/
static void LED_Init(void)
{
    /* Bật clock cho GPIOC */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    /* Cấu hình PC13: Output Push-Pull, tốc độ 2MHz */
    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio);
}

void LED_Toggle(void)
{
    /* XOR bit 13 của Output Data Register → đảo trạng thái LED */
    GPIOC->ODR ^= GPIO_Pin_13;
}

/* ===========================================================
 * BSW_Init – Khởi tạo BSW stack theo thứ tự AUTOSAR
 * -----------------------------------------------------------
 * Thứ tự khởi tạo bắt buộc (bottom-up):
 *
 *   1. MCAL (Microcontroller Abstraction Layer)
 *      → Can_Init() : Cấu hình bxCAN + GPIO (PA11, PA12)
 *
 *   2. ECU Abstraction Layer
 *      → CanIf_Init() : Khởi tạo CAN Interface
 *      → LinIf_Init() : Khởi tạo LIN Interface
 *                        (bên trong gọi Lin_Init() cho USART2)
 *
 *   3. Service Layer
 *      → PduR_Init()  : Khởi tạo PDU Router với routing table
 *      → Com_Init()   : Khởi tạo COM module (clear shadow buffers)
 *
 * Lưu ý: Thứ tự quan trọng! Module tầng trên phụ thuộc vào
 * module tầng dưới. Nếu init sai thứ tự, API sẽ trả lỗi.
 * ===========================================================*/
static void BSW_Init(void)
{
    /* 1. MCAL: CAN Driver – cấu hình bxCAN (CAN1) */
    Can_Init();

    /* 2. ECU Abstraction: CAN Interface */
    CanIf_Init();

    /* 3. ECU Abstraction: LIN Interface
     * (LinIf_Init bên trong gọi Lin_Init → cấu hình USART2) */
    LinIf_Init();

    /* 4. Service: Transport Protocol */
    CanTp_Init();

    /* 5. Service: PDU Router – nạp bảng routing CAN + LIN */
    PduR_Init(&PduR_ConfigPB);

    /* 5. Service: COM module – clear shadow buffers */
    Com_Init();
}

/* ===========================================================
 * Demo_CAN_EngineCmd – Gửi tín hiệu điều khiển động cơ
 * -----------------------------------------------------------
 * I-PDU: EngineCmd (CAN ID 0x180, DLC = 5 byte)
 *
 * Layout byte trong I-PDU:
 * ┌────────┬────────┬────────────┬───────┬─────┐
 * │ Byte 0 │ Byte 1 │   Byte 2   │ Byte 3      │ Byte 4
 * │Throttle│ Start  │TorqueLimit │Alive│CRC   │ (unused)
 * │ 8-bit  │ 1-bit  │  8-bit     │4-bit│4-bit │
 * └────────┴────────┴────────────┴───────┴─────┘
 *
 * Luồng: Com_SendSignal() → Com_TriggerIPDUSend()
 *         → PduR → CanIf → CAN Driver → bxCAN → CAN bus
 * ===========================================================*/
static void Demo_CAN_EngineCmd(void)
{
    /* Throttle: 75% (0..100%) → byte 0 */
    uint8 throttle = 75u;
    Com_SendSignal(ComSig_Engine_Throttle, &throttle);

    /* Engine Start: TRUE (1-bit) → byte 1, bit 0 */
    boolean engineStart = TRUE;
    Com_SendSignal(ComSig_Engine_Start, &engineStart);

    /* Torque Limit: 200 Nm → byte 2 */
    uint8 torqueLimit = 200u;
    Com_SendSignal(ComSig_Engine_TorqueLimit, &torqueLimit);

    /* Alive Counter: 4-bit (0..15), tự tăng mỗi lần gửi → byte 3 bit [0..3] */
    static uint8 alive = 0u;
    alive = (alive + 1u) & 0x0Fu;  /* Wrap around tại 16 */
    Com_SendSignal(ComSig_Engine_Alive, &alive);

    /* CRC: XOR đơn giản các giá trị → byte 3 bit [4..7] */
    uint8 crc = (uint8)(throttle ^ torqueLimit ^ alive) & 0x0Fu;
    Com_SendSignal(ComSig_Engine_CRC, &crc);

    /* Kích phát gửi I-PDU qua COM → PduR → CanIf → bxCAN */
    Com_TriggerIPDUSend(ComConf_ComIPdu_EngineCmd);
}

/* ===========================================================
 * Demo_CAN_BrakeCmd – Gửi tín hiệu điều khiển phanh
 * -----------------------------------------------------------
 * I-PDU: BrakeCmd (CAN ID 0x280, DLC = 3 byte)
 *
 * Layout byte:
 * ┌────────┬────────┬───────────┐
 * │ Byte 0 │ Byte 1 │  Byte 2   │
 * │BrakeReq│RegenReq│Alive│CRC  │
 * │ 8-bit  │ 8-bit  │4-bit│4-bit│
 * └────────┴────────┴───────────┘
 * ===========================================================*/
static void Demo_CAN_BrakeCmd(void)
{
    /* Brake Request: 30% → byte 0 */
    uint8 brakeReq = 30u;
    Com_SendSignal(ComSig_Brake_BrakeReq, &brakeReq);

    /* Regen Braking: 50% → byte 1 */
    uint8 regenReq = 50u;
    Com_SendSignal(ComSig_Brake_RegenReq, &regenReq);

    /* Alive Counter → byte 2 bit [0..3] */
    static uint8 alive = 0u;
    alive = (alive + 1u) & 0x0Fu;
    Com_SendSignal(ComSig_Brake_Alive, &alive);

    /* CRC → byte 2 bit [4..7] */
    uint8 crc = (uint8)(brakeReq ^ regenReq ^ alive) & 0x0Fu;
    Com_SendSignal(ComSig_Brake_CRC, &crc);

    /* Gửi I-PDU */
    Com_TriggerIPDUSend(ComConf_ComIPdu_BrakeCmd);
}

/* ===========================================================
 * Demo_CAN_BodyCmd – Gửi tín hiệu điều khiển thân xe
 * -----------------------------------------------------------
 * I-PDU: BodyCmd (CAN ID 0x380, DLC = 4 byte)
 *
 * Layout byte 0 (bit fields):
 * ┌─────┬─────┬─────┬─────┬─────────────┐
 * │bit 0│bit 1│bit 2│bit 3│ bit 4..7    │
 * │Head │TurnL│TurnR│Door │  (unused)   │
 * │lamp │     │     │Lock │             │
 * └─────┴─────┴─────┴─────┴─────────────┘
 * ===========================================================*/
static void Demo_CAN_BodyCmd(void)
{
    /* Headlamp: ON → byte 0, bit 0 */
    boolean headlamp = TRUE;
    Com_SendSignal(ComSig_Body_Headlamp, &headlamp);

    /* Turn Left: ON → byte 0, bit 1 */
    boolean turnL = TRUE;
    Com_SendSignal(ComSig_Body_TurnL, &turnL);

    /* Turn Right: OFF → byte 0, bit 2 */
    boolean turnR = FALSE;
    Com_SendSignal(ComSig_Body_TurnR, &turnR);

    /* Door Lock: ON → byte 0, bit 3 */
    boolean doorLock = TRUE;
    Com_SendSignal(ComSig_Body_DoorLock, &doorLock);

    /* Gửi I-PDU */
    Com_TriggerIPDUSend(ComConf_ComIPdu_BodyCmd);
}

/* ===========================================================
 * Demo_LIN_LightCtrl – Gửi tín hiệu điều khiển đèn (LIN)
 * -----------------------------------------------------------
 * I-PDU: LightCtrl (LIN ID 0x10, DLC = 4 byte)
 *
 * Luồng routing kiểu KÉO (PULL) khác CAN:
 *   App -> Cập nhật buffer COM -> Để đó.
 *   Khi Schedule Table của LinIf đến giờ:
 *   LinIf -> PduR -> gọi TriggerTransmit lôi data từ COM xuống
 *   -> Định dạng LIN Frame -> LIN Driver (USART2) -> LIN bus
 *
 * Layout byte:
 * ┌─────────┬──────────┬────────────┬────────┐
 * │ Byte 0  │          │   Byte 1   │Byte 2-3│
 * │Head│DRL │          │ Brightness │(unused) │
 * │1bit│1bit│          │  8-bit     │         │
 * └─────────┘          └────────────┴────────┘
 * ===========================================================*/
static void Demo_LIN_LightCtrl(void)
{
    /* Headlamp: ON → byte 0, bit 0 */
    boolean headlamp = TRUE;
    Com_SendSignal(ComSig_Light_Headlamp, &headlamp);

    /* DRL (Daytime Running Light): ON → byte 0, bit 1 */
    boolean drl = TRUE;
    Com_SendSignal(ComSig_Light_DRL, &drl);

    /* Brightness: 80% → byte 1 */
    uint8 brightness = 80u;
    Com_SendSignal(ComSig_Light_Brightness, &brightness);

    /* Chú ý: ĐỐI VỚI LIN UNCONDITIONAL FRAME, KHÔNG GỌI Com_TriggerIPDUSend() TẠI ĐÂY!
 *       Dữ liệu đã nằm trong COM Shadow Buffer.
 *       LinIf sẽ chủ động dùng lệnh kéo (TriggerTransmit) khi đến lược trong lịch trình */
}

/* ===========================================================
 * Demo_LIN_HVACCtrl – Gửi tín hiệu điều khiển điều hòa (LIN)
 * -----------------------------------------------------------
 * I-PDU: HVACCtrl (LIN ID 0x11, DLC = 3 byte)
 *
 * Layout byte:
 * ┌────────┬────────┬────────┐
 * │ Byte 0 │ Byte 1 │ Byte 2 │
 * │FanSpeed│(unused)│(unused)│
 * │ 8-bit  │        │        │
 * └────────┴────────┴────────┘
 * ===========================================================*/
static void Demo_LIN_HVACCtrl(void)
{
    /* Fan Speed: 128 (50%) → byte 0 */
    uint8 fanSpeed = 128u;
    Com_SendSignal(ComSig_HVAC_FanSpeed, &fanSpeed);

    /* Không dùng ép đẩy (Push) giống CAN
 *    Com_TriggerIPDUSend(ComConf_ComIPdu_HVACCtrl);
 *    Nhường lại cho LinIf Schedule kéo sau */
}

/* ===========================================================
 * Demo_CANTP_DiagTx – Demo gửi dữ liệu phân mảnh qua CanTp
 * -----------------------------------------------------------
 * Dữ liệu payload 20 byte sẽ được chia thành 1 FF và nhiều CF.
 * ===========================================================*/
static void Demo_CANTP_DiagTx(void)
{
    static uint8 counter = 0;

    /* Gửi message DiagTx dài 20 byte: lần đầu ngay, sau đó mỗi 10 vòng */
    if (counter % 10 == 0) {
        uint8 diagData[20] = {
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
            0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05
        };

        PduInfoType tpInfo;
        tpInfo.SduDataPtr = diagData;
        tpInfo.SduLength  = sizeof(diagData);

        /* Gửi thẳng xuống CanTp để demo (thực tế có thể đi từ Dcm qua PduR) */
        CanTp_Transmit(CanTpConf_CanTpTxNSdu_DiagTx, &tpInfo);
    }
    counter++;
}

/* ===========================================================
 * main – Điểm vào chương trình
 * -----------------------------------------------------------
 * Quy trình:
 *   1. Khởi tạo LED (PC13)
 *   2. Khởi tạo BSW stack (MCAL → ECU Abs → Service)
 *   3. Vòng lặp vô hạn:
 *      a. Gửi 3 CAN signals (Engine, Brake, Body)
 *      b. Gửi 2 LIN signals (Light, HVAC)
 *      c. Polling: Can_MainFunction_Write (TX confirm)
 *      d. Polling: LinIf_MainFunction (gửi pending LIN frames)
 *      e. Toggle LED (xác nhận đang chạy)
 *      f. Delay 100ms
 * ===========================================================*/
int main(void)
{
    /* Khởi tạo UART in log */
    Log_Init();
    Log_Print("\r\n--- AUTOSAR Classic COM Demo Booted ---\r\n");

    /* Khởi tạo LED chỉ thị hoạt động */
    LED_Init();

    /* Khởi tạo BSW stack (AUTOSAR Classic) */
    BSW_Init();

    /* === Vòng lặp chính – gửi CAN + LIN signals mỗi 100ms === */
    while (1) {

        /* ---- CANTP Demo ---- */
        /* Gửi COM bình thường: */
        Demo_CAN_EngineCmd();    /* CAN ID 0x180, DLC=5 */
        Demo_CAN_BrakeCmd();     /* CAN ID 0x280, DLC=3 */
        Demo_CAN_BodyCmd();      /* CAN ID 0x380, DLC=4 */
        Demo_LIN_LightCtrl();    /* LIN ID 0x10, DLC=4 */
        Demo_LIN_HVACCtrl();     /* LIN ID 0x11, DLC=3 */

        /* Truyền bản tin nặng */
        Demo_CANTP_DiagTx();


#ifdef NODE_RX
        /* TX Demo ko làm gì, CanTp_RxIndication sẽ in ra/toggle LED */
#endif

        /* ---- Polling main functions ---- */
        CanTp_MainFunction();      /* Quản lý state, timers CANTP      */
        Can_MainFunction_Read();   /* Lấy dữ liệu RX từ CAN Driver     */
        Can_MainFunction_Write();  /* Kiểm tra TX complete từ bxCAN    */
        LinIf_MainFunction();      /* Gửi pending LIN frames từ buffer */

        /* Toggle LED – xác nhận chương trình đang chạy */
        LED_Toggle();

        /* Delay 100ms trước vòng lặp tiếp theo */
        delay_ms(100u);
    }

    return 0;
}
