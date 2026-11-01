/**********************************************************
 * @file    CanIf.h
 * @brief   AUTOSAR CAN Interface (CanIf) – Header
 *
 * @details Tầng ECU Abstraction trừu tượng hóa CAN Driver (MCAL)
 *          cho các module phía trên (PduR/COM).
 *
 *          Vị trí trong kiến trúc AUTOSAR:
 *          ┌────────────────────┐
 *          │  COM / PduR        │  ← Tầng Service
 *          ├────────────────────┤
 *          │  CanIf (file này)  │  ← Tầng ECU Abstraction
 *          ├────────────────────┤
 *          │  CAN Driver (bxCAN)│  ← Tầng MCAL
 *          └────────────────────┘
 *
 *          Chức năng chính:
 *            - CanIf_Transmit()       : PduR gọi xuống → chuyển cho CAN Driver
 *            - CanIf_TxConfirmation() : CAN Driver gọi lên → chuyển cho PduR
 *            - CanIf_TriggerTransmit(): CAN Driver yêu cầu data → hỏi PduR
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef CANIF_H
#define CANIF_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "CanIf_Cfg.h"         /* Bảng cấu hình TX PDU */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Khởi tạo CanIf module
 * @details Set cờ CanIf_Initialized = TRUE.
 *          Production code sẽ load cấu hình, quản lý state controller.
 */
void CanIf_Init(void);

/**
 * @brief   PduR gọi: yêu cầu gửi I-PDU qua CAN bus
 * @param   CanIfTxPduId  ID PDU phía CanIf (từ bảng route PduR)
 * @param   PduInfoPtr    Dữ liệu PDU (payload + length)
 * @return  E_OK: CAN Driver chấp nhận, E_NOT_OK: lỗi
 *
 * @details Quy trình:
 *          1. Tra bảng CanIf_TxPduCfg → tìm CAN ID, HTH, DLC
 *          2. Dựng Can_PduType từ PduInfoType + config
 *          3. Gọi Can_Write(HTH, &canPdu) xuống CAN Driver
 */
Std_ReturnType CanIf_Transmit(PduIdType CanIfTxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief   Callback từ CAN Driver: TX đã hoàn tất
 * @param   CanTxPduId  Handle đã truyền (swPduHandle trong Can_PduType)
 * @details CanIf chuyển tiếp callback lên PduR → COM
 *          để thông báo I-PDU đã gửi thành công.
 */
void CanIf_TxConfirmation(PduIdType CanTxPduId);

/**
 * @brief   CAN Driver yêu cầu dữ liệu (pull model)
 * @param   CanTxPduId  PDU ID cần dữ liệu
 * @param   PduInfoPtr  [out] Buffer để điền dữ liệu
 * @return  E_OK: có dữ liệu, E_NOT_OK: không có
 * @details CanIf hỏi PduR → PduR hỏi COM → COM copy từ shadow buffer
 */
Std_ReturnType CanIf_TriggerTransmit(PduIdType CanTxPduId, PduInfoType* PduInfoPtr);

/**
 * @brief   Callback từ CAN Driver: RX đã hoàn tất
 * @param   canId      CAN ID của frame nhận được
 * @param   PduInfoPtr Thông tin frame (payload + length)
 * @details Tra CAN ID để routing: Diagnostic → CanTp, COM → PduR
 */
void CanIf_RxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif
#endif /* CANIF_H */
