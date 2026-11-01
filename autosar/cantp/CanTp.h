/**********************************************************
 * @file    CanTp.h
 * @brief   AUTOSAR CAN Transport Protocol (CanTp) – Header
 *
 * @details Xử lý phân mảnh (Segmentation) và lắp ráp
 *          (Reassembly) các N-PDU có kích thước lớn (>8 bytes)
 *          cho mạng CAN, theo tiêu chuẩn ISO 15765-2.
 *
 *          Vị trí trong kiến trúc:
 *          ┌────────────────────┐
 *          │       PduR         │
 *          ├────────────────────┤
 *          │      CanTp         │
 *          ├────────────────────┤
 *          │      CanIf         │
 *          └────────────────────┘
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef CANTP_H
#define CANTP_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "CanTp_Cfg.h"         /* Bảng cấu hình N-SDU / N-PDU */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Khởi tạo module CanTp
 * @details Đặt tất cả state machine của TX và RX về trạng thái IDLE,
 *          khởi tạo các thông số ISO-TP.
 */
void CanTp_Init(void);

/**
 * @brief   Main Function của CanTp
 * @details Hàm gọi theo chu kỳ (ví dụ: 5ms hoặc 10ms) để xử lý timeout
 *          (N_As, N_Bs, N_Cs, N_Ar, N_Br, N_Cr) và quản lý việc gửi
 *          Consecutive Frames (CF), gửi Flow Control (FC).
 */
void CanTp_MainFunction(void);

/**
 * @brief   PduR yêu cầu truyền dữ liệu (PduR -> CanTp)
 * @param   CanTpTxSduId ID của N-SDU muốn gửi (từ cấu hình)
 * @param   CanTpTxInfoPtr Con trỏ chứa độ dài và data (có thể null nếu data kéo sau)
 * @return  E_OK nếu chấp nhận, E_NOT_OK nếu bận hoặc lỗi
 */
Std_ReturnType CanTp_Transmit(PduIdType CanTpTxSduId, const PduInfoType* CanTpTxInfoPtr);

/**
 * @brief   CanIf báo đã nhận một CAN Frame (CanIf -> CanTp)
 * @param   CanTpRxPduId ID của N-PDU nhận (từ cấu hình CanTp)
 * @param   CanTpRxPduPtr Thông tin data nhận được (độ dài thực tế <= 8 byte, hoặc 64 byte với CAN-FD)
 */
void CanTp_RxIndication(PduIdType CanTpRxPduId, const PduInfoType* CanTpRxPduPtr);

/**
 * @brief   CanIf báo đã phát xong một CAN Frame (CanIf -> CanTp)
 * @param   CanTpTxPduId ID của N-PDU truyền (từ cấu hình CanTp)
 */
void CanTp_TxConfirmation(PduIdType CanTpTxPduId);


/* ===== Export Status Variable for Demo ===== */
extern boolean CanTp_Initialized;

#ifdef __cplusplus
}
#endif
#endif /* CANTP_H */
