/**********************************************************
 * @file    Lin.c
 * @brief   AUTOSAR LIN Driver – USART2 (STM32F103 SPL)
 *
 * @details Module MCAL điều khiển giao tiếp LIN (Local Interconnect
 *          Network) thông qua USART2 của STM32F103 sử dụng SPL.
 *
 *          ╔═══════════════════════════════════════════════╗
 *          ║           CẤU TRÚC LIN FRAME                 ║
 *          ╠═══════════════════════════════════════════════╣
 *          ║                                               ║
 *          ║  Header (Master gửi)    Response              ║
 *          ║  ┌──────┬──────┬─────┐  ┌──────────┬────────┐║
 *          ║  │Break │Sync  │ PID │  │Data 1..8 │Checksum│║
 *          ║  │13-bit│ 0x55 │     │  │  bytes   │        │║
 *          ║  │domin.│      │     │  │          │        │║
 *          ║  └──────┴──────┴─────┘  └──────────┴────────┘║
 *          ║                                               ║
 *          ║  Giao tiếp nối tiếp qua USART2:              ║
 *          ║    PA2 = TX → LIN Transceiver → LIN Bus      ║
 *          ║    PA3 = RX ← LIN Transceiver ← LIN Bus      ║
 *          ║    Baud: 19200 (chuẩn LIN 2.x)               ║
 *          ╚═══════════════════════════════════════════════╝
 *
 *          Luồng truyền (TX):
 *            1. LinIf gọi Lin_SendFrame(channel, &pdu)
 *            2. Gửi Break field bằng USART_SendBreak()
 *            3. Gửi Sync byte (0x55) qua USART_SendData()
 *            4. Tính và gửi PID (6-bit ID + 2-bit parity)
 *            5. Gửi Data bytes (1..8 byte)
 *            6. Tính và gửi Checksum
 *
 *          STM32 USART đặc biệt hỗ trợ LIN mode:
 *            - Tạo Break field tự động (USART_SendBreak)
 *            - Phát hiện Break (LIN Break Detection)
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/

#include "Lin.h"
#include "stm32f10x_rcc.h"     /* SPL: Quản lý clock (RCC)          */
#include "stm32f10x_gpio.h"    /* SPL: Cấu hình chân GPIO           */
#include "stm32f10x_usart.h"   /* SPL: API điều khiển USART         */
#include <stddef.h>            /* NULL                               */

extern void Log_Print(const char* str);
/* ===========================================================
 * Định nghĩa phần cứng
 * -----------------------------------------------------------
 * LIN_USART      : USART2 (trên APB1 bus, clock 36MHz)
 * LIN_USART_PORT : GPIOA (chứa chân TX/RX của USART2)
 * LIN_TX_PIN     : PA2 – Chân truyền LIN
 * LIN_RX_PIN     : PA3 – Chân nhận LIN
 * LIN_BAUD       : 19200 bps (tốc độ chuẩn LIN 2.x)
 * ===========================================================*/
#define LIN_USART          USART2
#define LIN_USART_PORT     GPIOA
#define LIN_TX_PIN         GPIO_Pin_2
#define LIN_RX_PIN         GPIO_Pin_3
#define LIN_BAUD           19200u

/* ===========================================================
 * Biến trạng thái nội bộ
 * ===========================================================*/
static boolean        s_linInited  = FALSE;    /**< Cờ đã khởi tạo         */
static Lin_StatusType s_linStatus  = LIN_OK;   /**< Trạng thái hiện tại    */
static uint8          s_linRxBuf[8];           /**< Buffer nhận (cho RX)   */

/* ===========================================================
 * prv_USART_SendByte – Gửi 1 byte qua USART (blocking)
 * -----------------------------------------------------------
 * Quy trình:
 *   1. Chờ cờ TXE (TX Empty) = SET → thanh ghi truyền trống
 *   2. Ghi byte vào thanh ghi DR → USART bắt đầu truyền
 *   3. Chờ cờ TC (Transmission Complete) = SET → byte đã gửi xong
 *
 * Chú ý: Hàm blocking, CPU đợi cho đến khi byte truyền xong.
 * ===========================================================*/
static void prv_USART_SendByte(uint8 data)
{
    /* Chờ thanh ghi truyền trống */
    while (USART_GetFlagStatus(LIN_USART, USART_FLAG_TXE) == RESET) {}

    /* Ghi byte vào data register → bắt đầu truyền */
    USART_SendData(LIN_USART, data);

    char hex[16];
    hex[0] = '[';
    hex[1] = '0';
    hex[2] = 'x';
    hex[3] = "0123456789ABCDEF"[data >> 4];
    hex[4] = "0123456789ABCDEF"[data & 0x0F];
    hex[5] = ']'; hex[6] = ' '; hex[7] = '\0';
    Log_Print(hex);

    /* Chờ truyền xong hoàn toàn */
    while (USART_GetFlagStatus(LIN_USART, USART_FLAG_TC) == RESET) {}
}

/* ===========================================================
 * prv_SendBreak – Gửi LIN Break field
 * -----------------------------------------------------------
 * Break field là tín hiệu đặc biệt mở đầu mỗi LIN frame:
 *   - Master kéo bus xuống mức dominant (0) trong ≥13 bit
 *   - Tất cả slave nhận Break → biết master bắt đầu frame mới
 *
 * STM32 USART hỗ trợ tạo Break tự động qua USART_SendBreak().
 * ===========================================================*/
static void prv_SendBreak(void)
{
    Log_Print("[UART2] [Break] ");
    /* Yêu cầu USART tạo Break field (13-bit dominant) */
    USART_SendBreak(LIN_USART);

    /* Chờ Break truyền xong */
    while (USART_GetFlagStatus(LIN_USART, USART_FLAG_TC) == RESET) {}
}

/* ===========================================================
 * prv_CalcPID – Tính Protected Identifier
 * -----------------------------------------------------------
 * LIN PID = 6-bit Frame ID + 2-bit Parity:
 *
 *   ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 *   │ P1  │ P0  │ ID5 │ ID4 │ ID3 │ ID2 │ ID1 │ ID0 │
 *   └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
 *   bit7   bit6
 *
 * Công thức parity:
 *   P0 = ID0 ⊕ ID1 ⊕ ID2 ⊕ ID4
 *   P1 = ¬(ID1 ⊕ ID3 ⊕ ID4 ⊕ ID5)
 *
 * Ví dụ: ID = 0x10 (010000)
 *   P0 = 0⊕0⊕0⊕1 = 1
 *   P1 = ¬(0⊕0⊕1⊕0) = ¬1 = 0
 *   PID = 0b01_010000 = 0x50
 * ===========================================================*/
static uint8 prv_CalcPID(uint8 id)
{
    uint8 id6 = id & 0x3Fu;  /* Lấy 6-bit thấp (Frame ID) */

    /* Tính P0: XOR các bit ID0, ID1, ID2, ID4 */
    uint8 p0 = ((id6 >> 0) ^ (id6 >> 1) ^ (id6 >> 2) ^ (id6 >> 4)) & 0x01u;

    /* Tính P1: đảo của XOR các bit ID1, ID3, ID4, ID5 */
    uint8 p1 = (~((id6 >> 1) ^ (id6 >> 3) ^ (id6 >> 4) ^ (id6 >> 5))) & 0x01u;

    /* Ghép: PID = P1:P0:ID5..ID0 */
    return (uint8)(id6 | (p0 << 6) | (p1 << 7));
}

/* ===========================================================
 * prv_CalcChecksum – Tính LIN Checksum
 * -----------------------------------------------------------
 * Hai loại checksum:
 *
 *   Classic (LIN 1.x):
 *     sum = Σ Data[i], carry handling
 *     checksum = ~sum & 0xFF
 *
 *   Enhanced (LIN 2.x):
 *     sum = PID + Σ Data[i], carry handling
 *     checksum = ~sum & 0xFF
 *
 * Carry handling: nếu sum ≥ 256 thì sum -= 255
 * (tương đương cộng carry bit vào kết quả)
 *
 * Ví dụ: PID=0x50, Data=[0x01, 0x02], Enhanced CS
 *   sum = 0x50 + 0x01 + 0x02 = 0x53
 *   checksum = ~0x53 = 0xAC
 * ===========================================================*/
static uint8 prv_CalcChecksum(uint8 pid, const uint8* data, uint8 len,
                               Lin_FrameCsModelType model)
{
    uint16 sum = 0u;

    /* Enhanced checksum: bắt đầu với PID */
    if (model == LIN_ENHANCED_CS) {
        sum = pid;
    }

    /* Cộng từng byte data với carry handling */
    for (uint8 i = 0; i < len; i++) {
        sum += data[i];
        if (sum >= 256u) {
            sum -= 255u;    /* Carry: sum = sum - 256 + 1 = sum - 255 */
        }
    }

    /* Đảo bit (invert) để tạo checksum */
    return (uint8)(~sum & 0xFFu);
}

/* ===========================================================
 * Lin_Init – Khởi tạo LIN Driver (USART2 + LIN mode)
 * -----------------------------------------------------------
 * Các bước:
 *   1. Bật clock: GPIOA, AFIO (APB2), USART2 (APB1)
 *   2. Cấu hình GPIO: PA2=TX (AF_PP), PA3=RX (Input)
 *   3. Cấu hình USART: 19200 baud, 8-bit, 1 stop
 *   4. Bật LIN mode: Break detection 11-bit
 *   5. Enable USART2
 * ===========================================================*/
void Lin_Init(void)
{
    /* Bước 1: Bật clock cho GPIO và USART2
     * USART2 nằm trên APB1 bus (clock 36MHz) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* Bước 2: Cấu hình chân TX (PA2) – Alternate Function Push-Pull
     * USART2 điều khiển chân này để truyền dữ liệu */
    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = LIN_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(LIN_USART_PORT, &gpio);

    /* Cấu hình chân RX (PA3) – Input Floating
     * Nhận tín hiệu từ LIN transceiver */
    gpio.GPIO_Pin  = LIN_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(LIN_USART_PORT, &gpio);

    /* Bước 3: Cấu hình USART2 cho LIN
     * Thông số: 19200 baud, 8 data bits, 1 stop bit, không parity
     * Chế độ: vừa TX vừa RX (full-duplex logical) */
    USART_InitTypeDef usart;
    USART_StructInit(&usart);
    usart.USART_BaudRate            = LIN_BAUD;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(LIN_USART, &usart);

    /* Bước 4: Bật chế độ LIN của USART
     * - USART_LINCmd: Bật LIN mode (USART tạo/phát hiện Break)
     * - USART_LINBreakDetectLength: độ dài Break detection = 11 bit
     *   (LIN spec yêu cầu Break ≥ 13 bit, detection ≥ 11 bit) */
    USART_LINCmd(LIN_USART, ENABLE);
    USART_LINBreakDetectLengthConfig(LIN_USART, USART_LINBreakDetectLength_11b);

    /* Bước 5: Enable USART2 */
    USART_Cmd(LIN_USART, ENABLE);

    /* Đánh dấu driver đã sẵn sàng */
    s_linInited = TRUE;
    s_linStatus = LIN_OK;
}

/* ===========================================================
 * Lin_SendFrame – Gửi LIN frame qua USART2
 * -----------------------------------------------------------
 * Trình tự gửi 1 LIN frame hoàn chỉnh:
 *
 *   ─────┐     ┌─────┬─────┬─────┬─────┬─────┬──────
 *        │Break│Sync │ PID │ D0  │ ... │ Check│
 *   ─────┘     └─────┴─────┴─────┴─────┴─────┴──────
 *    13 bit   1 byte  1 byte     1..8     1 byte
 *   dominant
 *
 * Hàm blocking: CPU đợi cho đến khi toàn bộ frame gửi xong.
 * ===========================================================*/
Std_ReturnType Lin_SendFrame(uint8 Channel, const Lin_PduType* PduInfoPtr)
{
    (void)Channel;  /* Demo chỉ dùng 1 kênh */

    /* Kiểm tra tham số đầu vào */
    if (!s_linInited || (PduInfoPtr == NULL) || (PduInfoPtr->SduPtr == NULL)) {
        return E_NOT_OK;
    }

    /* Kiểm tra độ dài dữ liệu hợp lệ (LIN: 1..8 byte) */
    if ((PduInfoPtr->Dl == 0u) || (PduInfoPtr->Dl > 8u)) {
        return E_NOT_OK;
    }

    /* Đánh dấu đang truyền */
    Log_Print("\r\n--- LIN MCAL Gửi Khung Mới ---\r\n");
    s_linStatus = LIN_TX_BUSY;

    /* === 1. Gửi Break field ===
     * Master kéo bus xuống dominant 13 bit để thông báo bắt đầu frame */
    prv_SendBreak();

    /* === 2. Gửi Sync byte (0x55) ===
     * Byte 01010101 giúp slave đồng bộ baud rate với master */
    prv_USART_SendByte(0x55u);

    /* === 3. Gửi PID (Protected Identifier) ===
     * Tính PID = 6-bit ID + 2-bit parity từ frame ID */
    uint8 pid = prv_CalcPID(PduInfoPtr->Pid & 0x3Fu);
    prv_USART_SendByte(pid);

    /* === 4. Gửi Data + Checksum (nếu Master TX) === */
    if (PduInfoPtr->Drc == LIN_FRAMERESPONSE_TX) {
        /* Master gửi response: data bytes */
        for (uint8 i = 0; i < PduInfoPtr->Dl; i++) {
            prv_USART_SendByte(PduInfoPtr->SduPtr[i]);
        }

        /* === 5. Gửi Checksum === */
        uint8 checksum = prv_CalcChecksum(pid, PduInfoPtr->SduPtr,
                                           PduInfoPtr->Dl, PduInfoPtr->Cs);
        prv_USART_SendByte(checksum);

        s_linStatus = LIN_TX_OK;
    } else if (PduInfoPtr->Drc == LIN_FRAMERESPONSE_RX) {
        /* RX response: master nhận response từ slave */
        /* 1. Flush USART RX buffer to remove loopbacked Sync and PID bytes (if transceiver loopback exists) */
        while (USART_GetFlagStatus(LIN_USART, USART_FLAG_RXNE) == SET) {
            (void)USART_ReceiveData(LIN_USART);
        }

        /* 2. Nhận Dl byte dữ liệu */
        boolean rx_success = TRUE;
        for (uint8 i = 0; i < PduInfoPtr->Dl; i++) {
            /* Trong môi trường thật, thêm timeout để tránh block vĩnh viễn */
            uint32 timeout = 0x00FFFFFF;
            while (USART_GetFlagStatus(LIN_USART, USART_FLAG_RXNE) == RESET) {
                if (--timeout == 0) {
                    rx_success = FALSE;
                    break;
                }
            }
            if (!rx_success) break;
            s_linRxBuf[i] = (uint8)USART_ReceiveData(LIN_USART);
        }

        if (rx_success) {
            /* 3. Nhận Checksum byte */
            uint32 timeout = 0x00FFFFFF;
            while (USART_GetFlagStatus(LIN_USART, USART_FLAG_RXNE) == RESET) {
                if (--timeout == 0) {
                    rx_success = FALSE;
                    break;
                }
            }
            if (rx_success) {
                uint8 received_checksum = (uint8)USART_ReceiveData(LIN_USART);
                /* 4. Kiểm tra Checksum */
                uint8 expected_checksum = prv_CalcChecksum(pid, s_linRxBuf, PduInfoPtr->Dl, PduInfoPtr->Cs);
                if (received_checksum == expected_checksum) {
                    s_linStatus = LIN_TX_OK;
                } else {
                    s_linStatus = LIN_TX_ERROR;
                    rx_success = FALSE;
                }
            }
        }

        if (!rx_success) {
            s_linStatus = LIN_TX_ERROR;
            return E_NOT_OK;
        }
    } else {
        /* LIN_FRAMERESPONSE_IGN: Bỏ qua response */
        s_linStatus = LIN_OK;
    }

    return E_OK;
}

/* ===========================================================
 * Lin_GetStatus – Lấy trạng thái hiện tại
 * ===========================================================*/
Lin_StatusType Lin_GetStatus(uint8 Channel, uint8** LinSduPtr)
{
    (void)Channel;

    /* Trả con trỏ buffer RX cho caller nếu cần */
    if (LinSduPtr != NULL) {
        *LinSduPtr = s_linRxBuf;
    }

    return s_linStatus;
}

/* ===========================================================
 * Lin_GoToSleep – Gửi Go-to-Sleep command
 * -----------------------------------------------------------
 * Theo LIN spec, master gửi diagnostic frame:
 *   - ID = 0x3C (Master Request Frame)
 *   - Data = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
 *   - Checksum = Classic (LIN 1.x compatible)
 *
 * Tất cả slave nhận frame này sẽ chuyển sang sleep mode,
 * giảm tiêu thụ năng lượng.
 * ===========================================================*/
Std_ReturnType Lin_GoToSleep(uint8 Channel)
{
    (void)Channel;
    if (!s_linInited) return E_NOT_OK;

    /* Dữ liệu go-to-sleep theo LIN spec */
    uint8 sleepData[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    /* Tạo frame go-to-sleep */
    Lin_PduType sleepPdu;
    sleepPdu.Pid    = 0x3Cu;          /* Diagnostic Master Request frame ID */
    sleepPdu.Cs     = LIN_CLASSIC_CS; /* Classic checksum cho diagnostic    */
    sleepPdu.Drc    = LIN_FRAMERESPONSE_TX;
    sleepPdu.Dl     = 8u;
    sleepPdu.SduPtr = sleepData;

    return Lin_SendFrame(Channel, &sleepPdu);
}

/* ===========================================================
 * Lin_Wakeup – Đánh thức bus LIN từ sleep mode
 * -----------------------------------------------------------
 * Theo LIN spec, bất kỳ node nào (master hoặc slave) có thể
 * đánh thức bus bằng cách kéo bus xuống dominant ≥250µs.
 *
 * Cách đơn giản: gửi byte 0xF0 qua USART ở 19200 baud.
 * Start bit (dominant) + 4 bit 0 = ~260µs dominant.
 * Sau wakeup, chờ bus ổn định ~150ms theo LIN spec.
 * ===========================================================*/
Std_ReturnType Lin_Wakeup(uint8 Channel)
{
    (void)Channel;
    if (!s_linInited) return E_NOT_OK;

    /* Gửi wakeup pulse: byte 0xF0 tạo ~260µs dominant */
    prv_USART_SendByte(0xF0u);

    /* Chờ bus ổn định (~150ms theo LIN spec) */
    for (volatile uint32 i = 0; i < 5400000u; i++) {}

    return E_OK;
}
