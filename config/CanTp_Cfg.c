#include "CanTp_Cfg.h"

const CanTp_TxNSduCfgType CanTp_TxSduCfg[CANTP_NUM_TX_SDUS] = {
    {
        .CanTpTxSduId = CanTpConf_CanTpTxNSdu_DiagTx,
        .CanIfTxPduId = 3, /* CanIf index 3 = DiagTx (CAN ID 0x7E0) */
        .PduRTxSduId = 0,
        .N_As = 1000,
        .N_Bs = 1000,
        .N_Cs = 90
    }
};

const CanTp_RxNSduCfgType CanTp_RxSduCfg[CANTP_NUM_RX_SDUS] = {
    {
        .CanTpRxSduId = CanTpConf_CanTpRxNSdu_DiagRx,
        .CanIfTxFcPduId = 4, /* CanIf index 4 = DiagRx_FC (CAN ID 0x7E0) */
        .PduRRxSduId = 0,
        .N_Ar = 1000,
        .N_Br = 1000,
        .N_Cr = 1000,
        .BlockSize = 8,
        .STmin = 20
    }
};
