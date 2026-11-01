#ifndef CANIF_TYPES_H
#define CANIF_TYPES_H

#include "Std_Types.h"
#include "ComStack_Types.h"

typedef uint16 CanIf_TxPduIdType;

typedef enum { CANIF_STANDARD_ID = 0, CANIF_EXTENDED_ID = 1 } CanIf_IdType_e;

typedef struct {
    CanIf_TxPduIdType TxPduId;
    uint32            CanId;         /* 11-bit hoặc 29-bit */
    CanIf_IdType_e    IdType;
    uint8             ControllerId;  /* nếu nhiều CAN ctrl */
    uint8             Hth;           /* handle TX hw/mailbox */
    uint8             Dlc;           /* 0..8 (nếu 0 + UseUpperLayerLen=TRUE ⇒ lấy theo info->SduLength) */
    boolean           UseUpperLayerLen;
} CanIf_TxPduCfgType;

/* PDU chuyển cho driver CAN (bxCAN/HAL/SPL) */
typedef struct {
    uint32   id;         /* CAN ID */
    uint8*   sdu;        /* payload ptr */
    uint8    length;     /* DLC 0..8 */
    uint32   swPduHandle;/* tx handle (tuỳ chọn) */
    uint8    isExtId;    /* 0: std, 1: ext */
} Can_PduType;

#endif /* CANIF_TYPES_H */
