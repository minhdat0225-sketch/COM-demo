/**********************************************************
 * @file    PduR_Cfg.c
 * @brief   Bảng route COM ↔ CanIf/LinIf
 * @details Routing table điều phối:
 *          - COM PDU 0,1,2 → CanIf (CAN bus)
 *          - COM PDU 3,4   → LinIf (LIN bus)
 **********************************************************/
#include <stddef.h>
#include "PduR_Cfg.h"
#include "Com_Cfg.h"
#include "CanIf_Cfg.h"

/* ===== LinIf PDU IDs (phía LinIf) ===== */
enum {
    LinIfConf_Pdu_LightCtrl = 0u,
    LinIfConf_Pdu_HVACCtrl  = 1u,
    LinIfConf_Pdu_TempSensor = 2u
};

/* ===== COM TX Routes: COM → CanIf hoặc LinIf ===== */
const PduR_Route1to1Type PduR_ComTxRoutes[PDUR_NUM_COM_TX_ROUTES] = {
    /* CAN routes */
    { ComConf_ComIPdu_EngineCmd,  CanIfConf_Pdu_EngineCmd, PDUR_DEST_CANIF },
    { ComConf_ComIPdu_BrakeCmd,   CanIfConf_Pdu_BrakeCmd,  PDUR_DEST_CANIF },
    { ComConf_ComIPdu_BodyCmd,    CanIfConf_Pdu_BodyCmd,   PDUR_DEST_CANIF },
    /* LIN routes */
    { ComConf_ComIPdu_LightCtrl,  LinIfConf_Pdu_LightCtrl, PDUR_DEST_LINIF },
    { ComConf_ComIPdu_HVACCtrl,   LinIfConf_Pdu_HVACCtrl,  PDUR_DEST_LINIF },
    /* New: EngineStatus TX route */
    { ComConf_ComIPdu_EngineStatus, CanIfConf_Pdu_EngineStatus, PDUR_DEST_CANIF }
};

/* ===== CanIf → COM RX Routes: CanIf → COM ===== */
const PduR_CallbackRouteType PduR_CanIfRxRoutes[PDUR_NUM_CANIF_RX_ROUTES] = {
    { CanIfConf_Pdu_EngineStatus, ComConf_ComIPdu_EngineStatus },
    { CanIfConf_Pdu_EngineCmd, ComConf_ComIPdu_EngineCmd }
};

/* ===== LinIf → COM RX Routes: LinIf → COM ===== */
const PduR_CallbackRouteType PduR_LinIfRxRoutes[PDUR_NUM_LINIF_RX_ROUTES] = {
    { LinIfConf_Pdu_TempSensor, ComConf_ComIPdu_TempSensor }
};

/* ===== CanIf → COM callback routes (TxConfirmation) ===== */
const PduR_CallbackRouteType PduR_CanIfTxConfRoutes[PDUR_NUM_CANIF_TXCONF_ROUTES] = {
    { CanIfConf_Pdu_EngineCmd, ComConf_ComIPdu_EngineCmd },
    { CanIfConf_Pdu_BrakeCmd,  ComConf_ComIPdu_BrakeCmd  },
    { CanIfConf_Pdu_BodyCmd,   ComConf_ComIPdu_BodyCmd   },
    { CanIfConf_Pdu_EngineStatus, ComConf_ComIPdu_EngineStatus }
};

/* ===== CanIf → COM callback routes (TriggerTransmit) ===== */
const PduR_CallbackRouteType PduR_CanIfTrigTxRoutes[PDUR_NUM_CANIF_TRIGTX_ROUTES] = {
    { CanIfConf_Pdu_EngineCmd, ComConf_ComIPdu_EngineCmd },
    { CanIfConf_Pdu_BrakeCmd,  ComConf_ComIPdu_BrakeCmd  },
    { CanIfConf_Pdu_BodyCmd,   ComConf_ComIPdu_BodyCmd   },
    { CanIfConf_Pdu_EngineStatus, ComConf_ComIPdu_EngineStatus }
};

/* ===== LinIf → COM callback routes (TxConfirmation) ===== */
const PduR_CallbackRouteType PduR_LinIfTxConfRoutes[PDUR_NUM_LINIF_TXCONF_ROUTES] = {
    { LinIfConf_Pdu_LightCtrl, ComConf_ComIPdu_LightCtrl },
    { LinIfConf_Pdu_HVACCtrl,  ComConf_ComIPdu_HVACCtrl  }
};

/* ===== LinIf → COM callback routes (TriggerTransmit) ===== */
const PduR_CallbackRouteType PduR_LinIfTrigTxRoutes[PDUR_NUM_LINIF_TRIGTX_ROUTES] = {
    { LinIfConf_Pdu_LightCtrl, ComConf_ComIPdu_LightCtrl },
    { LinIfConf_Pdu_HVACCtrl,  ComConf_ComIPdu_HVACCtrl  }
};

/* ===== Post-Build Config ===== */
const PduR_PBConfigType PduR_ConfigPB = {
    .ComTxRoutingTable       = PduR_ComTxRoutes,
#if PDUR_NUM_CANIF_RX_ROUTES > 0
    .CanIfRxRoutingTable     = PduR_CanIfRxRoutes,
#else
    .CanIfRxRoutingTable     = NULL,
#endif
#if PDUR_NUM_LINIF_RX_ROUTES > 0
    .LinIfRxRoutingTable     = PduR_LinIfRxRoutes,
#else
    .LinIfRxRoutingTable     = NULL,
#endif
    .CanIfTxConfRoutingTable = PduR_CanIfTxConfRoutes,
    .CanIfTrigTxRoutingTable = PduR_CanIfTrigTxRoutes,
    .ConfigId                = 0u
};

PduR_Route1to1Type PduR_DCM_Routes[PDUR_NUM_DCM_ROUTES] = {
    /* DCM routes sẽ được thêm vào đây nếu cần */
};
