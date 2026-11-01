/**********************************************************
 * @file    PduR_Cfg.h
 * @brief   PduR Routing Configuration – CAN + LIN
 * @details Bảng route hỗ trợ routing COM PDU tới CanIf HOẶC LinIf
 *          dựa trên trường DestModule trong cấu hình.
 *
 *          COM → PduR → CanIf (CAN bus)
 *                     → LinIf (LIN bus)
 **********************************************************/
#ifndef PDUR_CFG_H
#define PDUR_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "PduR.h"

/* ===== Destination Module Type ===== */
typedef enum {
    PDUR_DEST_CANIF = 0,   /* Route tới CanIf (CAN bus) */
    PDUR_DEST_LINIF = 1,   /* Route tới LinIf (LIN bus) */
    PDUR_DEST_CANTP = 2    /* Route tới CanTp (TP payload) */
} PduR_DestModuleType;

/* ===== Route entry 1:1 với Destination Module ===== */
typedef struct {
    PduIdType            SrcPduId;    /* PDU ID nguồn (COM side) */
    PduIdType            DstPduId;    /* PDU ID đích (CanIf/LinIf side) */
    PduR_DestModuleType  DestModule;  /* Module đích: CANIF hoặc LINIF */
} PduR_Route1to1Type;

/* ===== Số lượng routes ===== */
#define PDUR_NUM_COM_TX_ROUTES       (6u)   /* 3 CAN + 2 LIN + 1 EngineStatus */
#define PDUR_NUM_CANIF_RX_ROUTES     (2u)   /* EngineStatus + VehicleCommand RX */
#define PDUR_NUM_CANIF_TXCONF_ROUTES (4u)
#define PDUR_NUM_CANIF_TRIGTX_ROUTES (4u)
#define PDUR_NUM_LINIF_TXCONF_ROUTES (2u)
#define PDUR_NUM_LINIF_TRIGTX_ROUTES (2u)
#define PDUR_NUM_LINIF_RX_ROUTES     (1u)   /* TempSensor RX */
#define PDUR_NUM_DCM_ROUTES          (0u)

/* ===== Route Tables (extern) ===== */
extern const PduR_Route1to1Type PduR_ComTxRoutes[PDUR_NUM_COM_TX_ROUTES];

/* CanIf callback routes (không cần DestModule vì luôn lên COM) */
typedef struct {
    PduIdType SrcPduId;
    PduIdType DstPduId;
} PduR_CallbackRouteType;

#if PDUR_NUM_CANIF_RX_ROUTES > 0
extern const PduR_CallbackRouteType PduR_CanIfRxRoutes[PDUR_NUM_CANIF_RX_ROUTES];
#endif

#if PDUR_NUM_LINIF_RX_ROUTES > 0
extern const PduR_CallbackRouteType PduR_LinIfRxRoutes[PDUR_NUM_LINIF_RX_ROUTES];
#endif

extern const PduR_CallbackRouteType PduR_CanIfTxConfRoutes[PDUR_NUM_CANIF_TXCONF_ROUTES];
extern const PduR_CallbackRouteType PduR_CanIfTrigTxRoutes[PDUR_NUM_CANIF_TRIGTX_ROUTES];
extern const PduR_CallbackRouteType PduR_LinIfTxConfRoutes[PDUR_NUM_LINIF_TXCONF_ROUTES];
extern const PduR_CallbackRouteType PduR_LinIfTrigTxRoutes[PDUR_NUM_LINIF_TRIGTX_ROUTES];

extern const PduR_PBConfigType PduR_ConfigPB;

#ifdef __cplusplus
}
#endif

#endif /* PDUR_CFG_H */
