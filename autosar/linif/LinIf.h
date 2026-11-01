/**********************************************************
 * @file    LinIf.h
 * @brief   AUTOSAR LIN Interface (LinIf)
 * @details Lớp trừu tượng giữa PduR và Lin Driver (MCAL).
 *          PduR gọi LinIf_Transmit() — LinIf gọi Lin_SendFrame().
 *
 * @version 2.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef LINIF_H
#define LINIF_H

#include "Std_Types.h"
#include "ComStack_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== API ===== */

/** @brief Khởi tạo LinIf module (và Lin Driver bên dưới) */
void LinIf_Init(void);

/**
 * @brief  Truyền I-PDU qua LIN (được PduR gọi)
 * @param  TxPduId     PDU ID phía LinIf
 * @param  PduInfoPtr  Dữ liệu PDU (payload/length)
 * @return E_OK / E_NOT_OK
 */
Std_ReturnType LinIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/** @brief Main function – xử lý schedule & gửi pending frames */
void LinIf_MainFunction(void);

/** @brief Đưa bus LIN vào sleep */
Std_ReturnType LinIf_GotoSleep(uint8 Channel);

/** @brief Đánh thức bus LIN */
Std_ReturnType LinIf_Wakeup(uint8 Channel);

#ifdef __cplusplus
}
#endif
#endif /* LINIF_H */
