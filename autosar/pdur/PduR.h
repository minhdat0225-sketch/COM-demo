/**********************************************************
 * @file    PduR.h
 * @brief   AUTOSAR PDU Router – Header tổng quát (non-TP demo)
 * @details Cung cấp lifecycle & tiện ích chung (Init, GetVersionInfo,
 *          Enable/DisableRouting, GetConfigurationId).
 *          API cụ thể cho từng module đặt ở header riêng:
 *            - PduR_Com.h    (upper: COM)
 *            - PduR_CanIf.h  (lower: CanIf)
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef PDUR_H
#define PDUR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"        /* Std_ReturnType, Std_VersionInfoType */
#include "ComStack_Types.h"   /* PduIdType, PduInfoType */

#define PDUR_VENDOR_ID      (0u)
#define PDUR_MODULE_ID      (51u)   /* theo AUTOSAR module list (ví dụ) */
#define PDUR_SW_MAJOR_VERSION (1u)
#define PDUR_SW_MINOR_VERSION (0u)
#define PDUR_SW_PATCH_VERSION (0u)

/* Trạng thái PduR (rút gọn theo SWS – State Management) */
typedef enum {
    PDUR_UNINIT = 0,
    PDUR_ONLINE = 1
} PduR_StateType;

/* ===== Development errors (rút gọn, theo bảng SWS_PduR_00100) ===== */
#define PDUR_E_INIT_FAILED                     ((uint8)0x00)
#define PDUR_E_UNINIT                          ((uint8)0x01)
#define PDUR_E_PDU_ID_INVALID                  ((uint8)0x02)
#define PDUR_E_ROUTING_PATH_GROUP_ID_INVALID   ((uint8)0x08)
#define PDUR_E_PARAM_POINTER                   ((uint8)0x09)

/* ===== Routing Group ID (demo) ===== */
typedef uint16 PduR_RoutingPathGroupIdType;

/* ===== Cấu hình Post-Build (PB) rút gọn =====
 * Thực tế PduR_PBConfigType chứa routing tables, buffer, options…
 * (tham chiếu SWS 8.2.1). Ở demo: chỉ giữ con trỏ bảng route non-TP. */
typedef struct {
    /* implementation-specific: trỏ đến bảng route COM→CanIf, CanIf→COM, v.v. */
    const void* ComTxRoutingTable;
    const void* CanIfRxRoutingTable;
    const void* LinIfRxRoutingTable;
    const void* CanIfTxConfRoutingTable;
    const void* CanIfTrigTxRoutingTable;
    uint32       ConfigId;   /* id duy nhất (Enable/DisableRouting dùng) */
} PduR_PBConfigType;

/* ===== API chung (SWS 8.3.1) ===== */
/** @brief Khởi tạo PDU Router với cấu hình PB. */
void PduR_Init(const PduR_PBConfigType* ConfigPtr);

/** @brief Bật routing theo RoutingPathGroup (demo: bật toàn cục). */
Std_ReturnType PduR_EnableRouting(PduR_RoutingPathGroupIdType id);

/** @brief Tắt routing theo RoutingPathGroup (demo: tắt toàn cục). */
Std_ReturnType PduR_DisableRouting(PduR_RoutingPathGroupIdType id);

/** @brief Trạng thái hiện tại của PduR. */
PduR_StateType PduR_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_H */
