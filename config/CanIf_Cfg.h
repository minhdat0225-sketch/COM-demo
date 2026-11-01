/**********************************************************
 * @file    CanIf_Cfg.h
 * @brief   Map Tx-PDU → HTH/CAN-ID cho IF path.
 **********************************************************/
#ifndef CANIF_CFG_H
#define CANIF_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

#define CANIF_NUM_TX_PDUS   (6u)

enum {
    CanIfConf_Pdu_EngineCmd   = 0u,
    CanIfConf_Pdu_BrakeCmd    = 1u,
    CanIfConf_Pdu_BodyCmd     = 2u,
    CanIfConf_Pdu_DiagTx      = 3u,
    CanIfConf_Pdu_DiagRx_FC   = 4u,
    CanIfConf_Pdu_EngineStatus = 5u
};

typedef struct {
    PduIdType        CanIfTxPduId;
    Can_HwHandleType Hth;
    Can_IdType       CanId;
    uint8            DlcMax;
} CanIf_TxPduCfgType;

extern const CanIf_TxPduCfgType CanIf_TxPduCfg[CANIF_NUM_TX_PDUS];
extern boolean CanIf_Initialized;

#ifdef __cplusplus
}
#endif

#endif /* CANIF_CFG_H */
