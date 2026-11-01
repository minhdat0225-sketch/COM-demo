/**********************************************************
 * @file    CanIf_Cfg.c
 * @brief   Cấu hình Tx-PDU cho CanIf (IF path).
 **********************************************************/
#include "CanIf_Cfg.h"
const CanIf_TxPduCfgType CanIf_TxPduCfg[CANIF_NUM_TX_PDUS] = {
    { .CanIfTxPduId = CanIfConf_Pdu_EngineCmd, .Hth = (Can_HwHandleType)0u, .CanId = (Can_IdType)0x180u, .DlcMax = 5u },
    { .CanIfTxPduId = CanIfConf_Pdu_BrakeCmd,  .Hth = (Can_HwHandleType)1u, .CanId = (Can_IdType)0x280u, .DlcMax = 3u },
    { .CanIfTxPduId = CanIfConf_Pdu_BodyCmd,   .Hth = (Can_HwHandleType)2u, .CanId = (Can_IdType)0x380u, .DlcMax = 4u },
    { .CanIfTxPduId = CanIfConf_Pdu_DiagTx,    .Hth = (Can_HwHandleType)3u, .CanId = (Can_IdType)0x7E0u, .DlcMax = 8u },
    { .CanIfTxPduId = CanIfConf_Pdu_DiagRx_FC, .Hth = (Can_HwHandleType)4u, .CanId = (Can_IdType)0x7E0u, .DlcMax = 8u },
    { .CanIfTxPduId = CanIfConf_Pdu_EngineStatus, .Hth = (Can_HwHandleType)5u, .CanId = (Can_IdType)0x181u, .DlcMax = 8u }
};

boolean CanIf_Initialized = FALSE;
