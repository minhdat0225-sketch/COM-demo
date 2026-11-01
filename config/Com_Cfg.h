/**********************************************************
 * @file    Com_Cfg.h
 * @brief   AUTOSAR COM – Cấu hình CAN + LIN I-PDU paths
 * @details Định danh I-PDU và Signal:
 *          - CAN: Engine (0x180), Brake (0x280), Body (0x380)
 *          - LIN: LightCtrl (LIN ID 0x10), HVACCtrl (LIN ID 0x11)
 **********************************************************/
#ifndef COM_CFG_H
#define COM_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ===== Alias tương thích cũ ===== */
#define ComConf_ComIPdu_VCU_Command           ComConf_ComIPdu_EngineCmd
#define ComConf_ComSignal_VCU_ThrottleReq_pct ComSig_Engine_Throttle
#define ComConf_ComSignal_VCU_GearSel         ComSig_Engine_Start
#define ComConf_ComSignal_VCU_DriveMode       ComSig_Engine_TorqueLimit
#define ComConf_ComSignal_VCU_Alive           ComSig_Engine_Alive
#define ComConf_ComSignal_VCU_BrakeActive     ComSig_Engine_CRC

/* ===== Số lượng ===== */
#define COM_NUM_IPDUS    (7u)    /* 3 CAN TX + 2 LIN TX + 1 CAN RX + 1 LIN RX */
#define COM_NUM_SIGNALS  (21u)   /* 13 CAN TX + 4 LIN TX + 2 CAN RX + 2 LIN RX */
#define COM_MAX_IPDU_LEN (8u)    /* max PDU length */

typedef enum {
    COM_PDU_DIR_TX = 0,
    COM_PDU_DIR_RX = 1
} Com_PduDirection_e;

typedef enum {
    COM_SIGTYPE_UINT8 = 0,
    COM_SIGTYPE_BOOLEAN
} Com_SignalType_e;

/* ===== I-PDU IDs (CAN + LIN) ===== */
enum {
    /* CAN I-PDUs */
    ComConf_ComIPdu_EngineCmd  = 0u,   /* CAN 0x180, DLC=5 */
    ComConf_ComIPdu_BrakeCmd   = 1u,   /* CAN 0x280, DLC=3 */
    ComConf_ComIPdu_BodyCmd    = 2u,   /* CAN 0x380, DLC=4 */
    /* LIN I-PDUs */
    ComConf_ComIPdu_LightCtrl  = 3u,   /* LIN ID 0x10, DLC=4 */
    ComConf_ComIPdu_HVACCtrl   = 4u,   /* LIN ID 0x11, DLC=3 */
    /* RX I-PDUs */
    ComConf_ComIPdu_EngineStatus = 5u, /* CAN 0x181, DLC=3 (RPM 16-bit, Temp 8-bit) */
    ComConf_ComIPdu_TempSensor   = 6u  /* LIN ID 0x20, DLC=2 (Temp 8-bit, Status 8-bit) */
};

typedef uint16 Com_SignalIdType;

/* ===== Signal IDs ===== */
enum {
    /* EngineCmd (CAN 0x180) DLC=5 */
    ComSig_Engine_Throttle    = 0u,
    ComSig_Engine_Start       = 1u,
    ComSig_Engine_TorqueLimit = 2u,
    ComSig_Engine_Alive       = 3u,
    ComSig_Engine_CRC         = 4u,

    /* BrakeCmd (CAN 0x280) DLC=3 */
    ComSig_Brake_BrakeReq = 5u,
    ComSig_Brake_RegenReq = 6u,
    ComSig_Brake_Alive    = 7u,
    ComSig_Brake_CRC      = 8u,

    /* BodyCmd (CAN 0x380) DLC=4 */
    ComSig_Body_Headlamp = 9u,
    ComSig_Body_TurnL    = 10u,
    ComSig_Body_TurnR    = 11u,
    ComSig_Body_DoorLock = 12u,

    /* LightCtrl (LIN 0x10) DLC=4 */
    ComSig_Light_Headlamp    = 13u,   /* ON/OFF */
    ComSig_Light_DRL         = 14u,   /* Daytime Running Light */
    ComSig_Light_Brightness  = 15u,   /* 0..100% */

    /* HVACCtrl (LIN 0x11) DLC=3 */
    ComSig_HVAC_FanSpeed     = 16u,   /* 0..255 */

    /* EngineStatus (CAN 0x181) DLC=3 - RX */
    ComSig_Engine_RPM        = 17u,   /* 16-bit, byte 0-1 */
    ComSig_Engine_Temp       = 18u,   /* 8-bit, byte 2 */

    /* TempSensor (LIN 0x20) DLC=2 - RX */
    ComSig_Temp_Value        = 19u,   /* 8-bit, byte 0 */
    ComSig_Temp_Status       = 20u    /* 8-bit, byte 1 */
};

typedef struct {
    PduIdType          PduId;
    PduLengthType      Length;
    Com_PduDirection_e direction;
} Com_IPduCfgType;

typedef struct {
    PduIdType          PduId;
    uint16             byteIndex;
    uint8              bitOffset;
    uint8              bitLength;
    Com_SignalType_e   type;
    Com_PduDirection_e direction;
} Com_SignalCfgType;

extern const Com_IPduCfgType   Com_IPduCfg[COM_NUM_IPDUS];
extern const Com_SignalCfgType Com_SignalCfg[COM_NUM_SIGNALS];

#ifdef __cplusplus
}
#endif

#endif /* COM_CFG_H */
