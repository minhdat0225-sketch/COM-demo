/**********************************************************
 * @file    Can_GeneralTypes.h
 * @brief   Kiểu dữ liệu chung cho CAN Stack (AUTOSAR)
 *
 * @details File này định nghĩa các kiểu dữ liệu được dùng
 *          chung giữa CAN Driver và CAN Interface (CanIf):
 *
 *          ┌───────────────────────────────────────────────┐
 *          │  Can_ReturnType    │ Mã trả về CAN Driver    │
 *          │  Can_IdType        │ CAN ID (11/29-bit)      │
 *          │  Can_HwHandleType  │ Handle phần cứng TX/RX  │
 *          │  Can_PduType       │ Mô tả CAN PDU khi TX    │
 *          └───────────────────────────────────────────────┘
 *
 *          Luồng sử dụng:
 *            CanIf → dựng Can_PduType → gọi Can_Write(HTH, &pdu)
 *            CAN Driver → đọc Can_PduType → ghi vào bxCAN mailbox
 *
 *          Tham chiếu: AUTOSAR_SWS_CANGeneralTypes
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/

#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ===========================================================
 * Can_ReturnType – Mã trả về của CAN Driver API
 * -----------------------------------------------------------
 * Dùng cho hàm Can_Write() và các API driver CAN khác.
 *
 * CAN_OK     : Yêu cầu thành công, đã nạp vào mailbox
 * CAN_NOT_OK : Lỗi (tham số sai, driver chưa init, v.v.)
 * CAN_BUSY   : Tất cả mailbox TX đang bận, thử lại sau
 * ===========================================================*/
typedef enum
{
    CAN_OK = 0,      /**< Thành công                           */
    CAN_NOT_OK,       /**< Lỗi chung                            */
    CAN_BUSY          /**< Mailbox TX bận, thử lại sau          */
} Can_ReturnType;

/* ===========================================================
 * Can_IdType – CAN Identifier
 * -----------------------------------------------------------
 * Chứa CAN ID:
 *   - Standard ID: 11-bit (0x000..0x7FF)
 *   - Extended ID: 29-bit (0x00000000..0x1FFFFFFF)
 *
 * Dùng uint32 để chứa cả hai loại.
 * Bit 31..29 có thể dùng làm cờ IDE/RTR tùy triển khai.
 * ===========================================================*/
typedef uint32 Can_IdType;

/* ===========================================================
 * Can_HwHandleType – Handle phần cứng CAN
 * -----------------------------------------------------------
 * Đại diện cho tài nguyên phần cứng TX/RX:
 *   - HTH (Hardware Transmit Handle): chỉ mailbox TX
 *   - HRH (Hardware Receive Handle) : chỉ bộ lọc RX
 *
 * CanIf dùng HTH khi gọi Can_Write() để chọn mailbox cụ thể.
 * STM32F103 bxCAN có 3 mailbox TX → HTH = 0, 1, 2.
 * ===========================================================*/
typedef uint16 Can_HwHandleType;

/* ===========================================================
 * Can_PduType – CAN Protocol Data Unit
 * -----------------------------------------------------------
 * Cấu trúc mà CanIf truyền xuống CAN Driver khi muốn gửi
 * một CAN frame. CAN Driver nhận qua Can_Write(HTH, &pdu).
 *
 * Các trường:
 *   swPduHandle : Handle phần mềm do CanIf cấp, dùng để
 *                 xác nhận ngược khi TX xong (callback)
 *   length      : DLC - Data Length Code (0..8 byte)
 *   id          : CAN ID (Standard 11-bit hoặc Extended 29-bit)
 *   sdu         : Con trỏ tới mảng byte payload
 *
 * Ví dụ sử dụng:
 *   Can_PduType frame;
 *   frame.id          = 0x180;      // CAN ID
 *   frame.length      = 5;          // 5 byte dữ liệu
 *   frame.sdu         = dataBuffer; // Con trỏ data
 *   frame.swPduHandle = pduId;      // Để callback
 *   Can_Write(0, &frame);           // Gửi qua mailbox 0
 * ===========================================================*/
typedef struct
{
    PduIdType   swPduHandle; /**< Handle do CanIf cấp, dùng khi TxConfirmation */
    uint8       length;      /**< DLC: số byte dữ liệu (0..8)                  */
    Can_IdType  id;          /**< CAN Identifier (11-bit hoặc 29-bit)           */
    uint8*      sdu;         /**< Con trỏ tới mảng payload                      */
} Can_PduType;

#ifdef __cplusplus
}
#endif

#endif /* CAN_GENERAL_TYPES_H */
