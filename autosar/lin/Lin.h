/**********************************************************
 * @file    Lin.h
 * @brief   AUTOSAR LIN Driver – USART2 (STM32F103)
 *
 * @details Module MCAL điều khiển giao tiếp LIN (Local
 *          Interconnect Network) thông qua USART2 của STM32F103.
 *
 *          LIN là giao thức truyền thông nối tiếp đơn dây (single-wire),
 *          dùng trong ô tô cho các thiết bị tốc độ thấp (đèn, gương,
 *          HVAC...). Master điều khiển bus, slave phản hồi.
 *
 *          Cấu trúc LIN frame:
 *          ┌───────┬──────┬─────┬──────────┬──────────┐
 *          │ Break │ Sync │ PID │ Data     │ Checksum │
 *          │ 13bit │ 0x55 │     │ 1..8 byte│          │
 *          └───────┴──────┴─────┴──────────┴──────────┘
 *
 *          Phần cứng:
 *            USART2: PA2 = TX, PA3 = RX
 *            Baud: 19200 (chuẩn LIN 2.x)
 *
 *          STM32 USART hỗ trợ LIN mode (tạo Break field tự động).
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef LIN_H
#define LIN_H

#include "Std_Types.h"
#include "ComStack_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================
 * Lin_StatusType – Trạng thái LIN Driver
 * -----------------------------------------------------------
 * LIN_OK      : Sẵn sàng, không có hoạt động
 * LIN_NOT_OK  : Lỗi chung
 * LIN_BUSY    : Đang truyền hoặc nhận
 * LIN_TX_OK   : Truyền frame xong thành công
 * LIN_TX_BUSY : Đang truyền frame
 * LIN_TX_ERROR: Truyền frame thất bại
 * ===========================================================*/
typedef enum {
    LIN_OK = 0,
    LIN_NOT_OK,
    LIN_BUSY,
    LIN_TX_OK,
    LIN_TX_BUSY,
    LIN_TX_ERROR
} Lin_StatusType;

/* ===========================================================
 * Lin_FrameCsModelType – Loại Checksum
 * -----------------------------------------------------------
 * LIN 1.x dùng Classic Checksum (chỉ data bytes).
 * LIN 2.x dùng Enhanced Checksum (PID + data bytes).
 *
 * Công thức: sum = Σ bytes, carry → sum -= 255
 *            checksum = ~sum & 0xFF
 * ===========================================================*/
typedef enum {
    LIN_ENHANCED_CS  = 0,     /**< Enhanced checksum (LIN 2.x): PID + Data */
    LIN_CLASSIC_CS   = 1      /**< Classic checksum (LIN 1.x): chỉ Data    */
} Lin_FrameCsModelType;

/* ===========================================================
 * Lin_FrameResponseType – Hướng dữ liệu trong frame
 * -----------------------------------------------------------
 * LIN_FRAMERESPONSE_TX  : Master gửi cả header + response (data)
 * LIN_FRAMERESPONSE_RX  : Master gửi header, slave gửi response
 * LIN_FRAMERESPONSE_IGN : Bỏ qua response
 * ===========================================================*/
typedef enum {
    LIN_FRAMERESPONSE_TX  = 0,  /**< Master gửi response          */
    LIN_FRAMERESPONSE_RX  = 1,  /**< Slave gửi response            */
    LIN_FRAMERESPONSE_IGN = 2   /**< Bỏ qua response               */
} Lin_FrameResponseType;

/* ===========================================================
 * Lin_PduType – Mô tả LIN PDU (Protocol Data Unit)
 * -----------------------------------------------------------
 * Cấu trúc chứa thông tin cần thiết để gửi/nhận 1 LIN frame.
 * LinIf sẽ tạo Lin_PduType rồi gọi Lin_SendFrame().
 *
 * Pid    : Protected Identifier (6-bit ID + 2-bit parity)
 * Cs     : Loại checksum (Enhanced/Classic)
 * Drc    : Hướng response (TX/RX/Ignore)
 * Dl     : Data Length (1..8 byte)
 * SduPtr : Con trỏ tới mảng dữ liệu
 * ===========================================================*/
typedef struct {
    uint8                  Pid;     /**< Protected Identifier             */
    Lin_FrameCsModelType   Cs;      /**< Loại checksum                    */
    Lin_FrameResponseType  Drc;     /**< Hướng dữ liệu                   */
    uint8                  Dl;      /**< Data Length (1..8)               */
    uint8*                 SduPtr;  /**< Con trỏ dữ liệu                 */
} Lin_PduType;

/* ===========================================================
 * API công khai
 * ===========================================================*/

/**
 * @brief   Khởi tạo LIN Driver
 * @details Cấu hình USART2 + GPIO cho LIN:
 *          - PA2 (TX): AF Push-Pull
 *          - PA3 (RX): Input Floating
 *          - Baud: 19200, 8-bit, 1 stop, no parity
 *          - Bật LIN mode (Break detection 11-bit)
 */
void Lin_Init(void);

/**
 * @brief   Gửi LIN frame qua USART2
 * @param   Channel     Kênh LIN (demo: chỉ dùng channel 0)
 * @param   PduInfoPtr  Mô tả frame: PID, data, checksum type
 * @return  E_OK: Gửi thành công, E_NOT_OK: Lỗi
 *
 * @details Trình tự gửi:
 *          1. Break field (13-bit dominant) → USART_SendBreak()
 *          2. Sync byte (0x55)
 *          3. PID (Protected Identifier)
 *          4. Data (1..8 bytes)
 *          5. Checksum (Enhanced hoặc Classic)
 */
Std_ReturnType Lin_SendFrame(uint8 Channel, const Lin_PduType* PduInfoPtr);

/**
 * @brief   Lấy trạng thái hiện tại của LIN Driver
 * @param   Channel    Kênh LIN
 * @param   LinSduPtr  [out] Con trỏ tới buffer RX (nếu có)
 * @return  Lin_StatusType: trạng thái hiện tại
 */
Lin_StatusType Lin_GetStatus(uint8 Channel, uint8** LinSduPtr);

/**
 * @brief   Đưa bus LIN vào Sleep mode
 * @param   Channel  Kênh LIN
 * @return  E_OK: Đã gửi go-to-sleep command
 * @details Gửi diagnostic frame (ID=0x3C) với data sleep pattern
 */
Std_ReturnType Lin_GoToSleep(uint8 Channel);

/**
 * @brief   Đánh thức bus LIN từ Sleep mode
 * @param   Channel  Kênh LIN
 * @return  E_OK: Đã gửi wakeup pulse
 * @details Kéo bus LIN về dominant ≥250µs theo LIN spec
 */
Std_ReturnType Lin_Wakeup(uint8 Channel);

#ifdef __cplusplus
}
#endif
#endif /* LIN_H */
