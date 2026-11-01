/**********************************************************
 * @file    PduR_Com.h
 * @brief   Giao diện PduR cho upper layer COM (non-TP)
 * @details Theo SWS COM & SWS PduR:
 *          - COM gửi I-PDU qua PduR_ComTransmit().
 *          - Khi lower cần data theo TriggerTransmit, PduR gọi
 *            Com_TriggerTransmit(). (callout chiều ngược)
 **********************************************************/
#ifndef PDUR_COM_H
#define PDUR_COM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ====== API cho COM (SWS_Com_00138 / bảng 7.6) ====== */
/**
 * @brief  COM yêu cầu truyền một I-PDU
 * @param  ComTxPduId   I-PDU ID phía COM
 * @param  PduInfoPtr   trỏ dữ liệu payload/length
 * @return E_OK / E_NOT_OK (tổng hợp kết quả từ lower, 1:n có thể khác)
 */
Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_COM_H */
