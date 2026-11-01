/**********************************************************
 * @file    PduR_CanIf.h
 * @brief   Giao diện PduR cho lower layer CanIf (non-TP)
 * @details Đây là các callback mà CanIf sẽ gọi lên PduR
 *          (SWS PduR 8.3.3 – Communication Interface):
 *            - RxIndication, TxConfirmation, TriggerTransmit.
 **********************************************************/
#ifndef PDUR_CANIF_H
#define PDUR_CANIF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* CanIf → PduR: I-PDU đã nhận xong (L-PDU RX done) */
void PduR_CanIfRxIndication(PduIdType CanIfRxPduId, const PduInfoType* PduInfoPtr);

/* CanIf → PduR: I-PDU TX đã xác nhận xong */
void PduR_CanIfTxConfirmation(PduIdType CanIfTxPduId);

/* CanIf → PduR: Lower yêu cầu upper cấp dữ liệu để phát (pull model) */
Std_ReturnType PduR_CanIfTriggerTransmit(PduIdType CanIfTxPduId, PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_CANIF_H */
