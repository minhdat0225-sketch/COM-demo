/**********************************************************
 * @file    CanIf.c
 * @brief   AUTOSAR CAN Interface (CanIf) – Triển khai TX path
 *
 * @details Tầng ECU Abstraction kết nối PduR (phía trên) với
 *          CAN Driver (phía dưới). Chịu trách nhiệm:
 *
 *          1. Chuyển đổi dữ liệu:
 *             PduInfoType (AUTOSAR chung) → Can_PduType (CAN riêng)
 *
 *          2. Tra bảng cấu hình:
 *             CanIf_TxPduCfg[] map CanIfTxPduId → CAN ID + HTH + DLC
 *
 *          3. Callback ngược:
 *             CAN Driver báo TX xong → CanIf → PduR → COM
 *
 *          Luồng TX chi tiết:
 *          ┌────────┐  CanIf_Transmit(id, &info)  ┌────────┐
 *          │  PduR  │ ──────────────────────────►  │ CanIf  │
 *          └────────┘                              └────┬───┘
 *                                                       │ Can_Write(hth, &pdu)
 *                                                  ┌────▼───────┐
 *                                                  │ CAN Driver │
 *                                                  │  (bxCAN)   │
 *                                                  └────────────┘
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/

#include "CanIf.h"
#include <stdint.h>
#include "Can.h"               /* Can_Write() – API CAN Driver       */
#include "PduR.h"
#include "PduR_CanIf.h"        /* PduR_CanIfTxConfirmation() callback */
#include <string.h>

__attribute__((weak)) void App_CanRxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    (void)canId;
    (void)PduInfoPtr;
}

/* ===========================================================
 * prv_find_txpdu – Tìm index TX PDU trong bảng cấu hình
 * -----------------------------------------------------------
 * Tra bảng CanIf_TxPduCfg[] để tìm entry có CanIfTxPduId
 * khớp với id truyền vào.
 *
 * @param  id   PDU ID cần tìm
 * @return index (0..N-1) nếu tìm thấy, -1 nếu không thấy
 * ===========================================================*/
static inline int16_t prv_find_txpdu(PduIdType id)
{
    for (uint16 i = 0; i < CANIF_NUM_TX_PDUS; ++i) {
        if (CanIf_TxPduCfg[i].CanIfTxPduId == id) {
            return (int16_t)i;
        }
    }
    return (int16_t)-1;
}

/* ===========================================================
 * CanIf_Init – Khởi tạo CanIf module
 * ===========================================================*/
void CanIf_Init(void)
{
    /* Đánh dấu CanIf đã được khởi tạo
     * Production code sẽ: load config, init state controller, v.v. */
    CanIf_Initialized = TRUE;
}

/* ===========================================================
 * CanIf_Transmit – PduR gọi: gửi I-PDU qua CAN bus
 * -----------------------------------------------------------
 * Quy trình chi tiết:
 *
 *   1. Kiểm tra CanIf đã init chưa, param hợp lệ không
 *   2. Tra bảng CanIf_TxPduCfg[] → tìm config cho PDU ID:
 *      ┌─────────────┬─────┬──────────┬────────┐
 *      │CanIfTxPduId │ HTH │  CanId   │ DlcMax │
 *      ├─────────────┼─────┼──────────┼────────┤
 *      │ 0 (Engine)  │ 0   │  0x180   │  5     │
 *      │ 1 (Brake)   │ 1   │  0x280   │  3     │
 *      │ 2 (Body)    │ 2   │  0x380   │  4     │
 *      └─────────────┴─────┴──────────┴────────┘
 *   3. Kiểm tra DLC không vượt quá DlcMax
 *   4. Dựng Can_PduType:
 *      - swPduHandle = CanIfTxPduId (để callback ngược)
 *      - id   = CanId từ config
 *      - length = SduLength từ PduInfo
 *      - sdu    = SduDataPtr từ PduInfo
 *   5. Gọi Can_Write(HTH, &canPdu)
 * ===========================================================*/
Std_ReturnType CanIf_Transmit(PduIdType CanIfTxPduId, const PduInfoType* PduInfoPtr)
{
    /* Bước 1: Kiểm tra điều kiện */
    if (!CanIf_Initialized) {
        return E_NOT_OK;
    }
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) {
        return E_NOT_OK;
    }

    /* Bước 2: Tra bảng TX PDU config */
    int16_t idx = prv_find_txpdu(CanIfTxPduId);
    if (idx < 0) { return E_NOT_OK; }

    const CanIf_TxPduCfgType* cfg = &CanIf_TxPduCfg[(uint16)idx];

    /* Bước 3: Kiểm tra DLC không vượt config */
    if (PduInfoPtr->SduLength > cfg->DlcMax) {
        return E_NOT_OK;
    }

    /* Bước 4: Dựng Can_PduType cho CAN Driver */
    Can_PduType frame;
    frame.swPduHandle = CanIfTxPduId;                 /* Handle để callback ngược */
    frame.length      = (uint8)PduInfoPtr->SduLength; /* DLC */
    frame.id          = cfg->CanId;                   /* CAN ID từ config */
    frame.sdu         = (uint8*)PduInfoPtr->SduDataPtr; /* Payload */

    /* Bước 5: Gọi xuống CAN Driver (MCAL)
     * Can_Write() trả về CAN_OK / CAN_BUSY / CAN_NOT_OK
     * Map sang Std_ReturnType: CAN_OK → E_OK, còn lại → E_NOT_OK */
    Can_ReturnType rc = Can_Write(cfg->Hth, &frame);
    return (rc == CAN_OK) ? E_OK : E_NOT_OK;
}

/* ===========================================================
 * CanIf_TxConfirmation – CAN Driver báo TX hoàn tất
 * -----------------------------------------------------------
 * Khi CAN Driver (Can_MainFunction_Write) phát hiện mailbox
 * đã truyền xong, nó gọi callback này.
 *
 * CanIf chuyển tiếp lên PduR:
 *   CanIf_TxConfirmation()
 *     → PduR_CanIfTxConfirmation()
 *       → Com_TxConfirmation()
 * ===========================================================*/
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    PduR_CanIfTxConfirmation(CanTxPduId);
}

/* ===========================================================
 * CanIf_TriggerTransmit – CAN Driver yêu cầu dữ liệu (pull)
 * -----------------------------------------------------------
 * Trong một số cấu hình AUTOSAR, CAN Driver không nhận data
 * trực tiếp từ Can_Write() mà "kéo" (pull) data khi cần.
 *
 * Luồng: CAN Driver → CanIf → PduR → COM (copy shadow buffer)
 * ===========================================================*/
Std_ReturnType CanIf_TriggerTransmit(PduIdType CanTxPduId, PduInfoType* PduInfoPtr)
{
    return PduR_CanIfTriggerTransmit(CanTxPduId, PduInfoPtr);
}

/* ===========================================================
 * CanIf_RxIndication – CAN Driver báo có RX Frame
 * -----------------------------------------------------------
 * canId: CAN ID của frame nhận được (dùng để routing)
 * ===========================================================*/
volatile Can_IdType CanIf_LastRxIds[10] = {0};
volatile uint32 CanIf_RxIdIndex = 0;

void CanIf_RxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    if (PduInfoPtr == NULL) return;

    CanIf_LastRxIds[CanIf_RxIdIndex % 10] = canId;
    CanIf_RxIdIndex++;
    App_CanRxIndication(canId, PduInfoPtr);

#ifdef EVCU_DIAG_ECU
    if (canId == 0x7E8) {
        return;
    }
#endif

    extern void CanTp_RxIndication(PduIdType CanTpRxPduId, const PduInfoType* CanTpRxPduPtr);

    /* Routing theo CAN ID */
    if (canId == 0x7DF || canId == 0x7E8 || canId == 0x7E0) {
        CanTp_RxIndication(0, PduInfoPtr);
    } else if (canId == 0x180) {
        PduR_CanIfRxIndication(CanIfConf_Pdu_EngineCmd, PduInfoPtr);
    } else if (canId == 0x181) {
        /* COM PDU: EngineStatus (giả định dùng chung PduId 5 cho RX) */
        PduR_CanIfRxIndication(CanIfConf_Pdu_EngineStatus, PduInfoPtr);
    }
}
