/**********************************************************
 * @file    Com.c
 * @brief   AUTOSAR COM – TX-only (MCU demo, SWS-style returns)
 * @details Dịch vụ TX tối thiểu:
 *          - Com_Init()
 *          - Com_SendSignal()    -> uint8: E_OK / COM_SERVICE_NOT_AVAILABLE / COM_BUSY
 *          - Com_TriggerIPDUSend()-> Std_ReturnType: E_OK / E_NOT_OK
 *
 *          Theo SWS:
 *            • Com_SendSignal (8.3.3.1): API trả uint8 với E_OK, COM_SERVICE_NOT_AVAILABLE,
 *              COM_BUSY; khi TRIGGERED + DIRECT/MIXED có thể phát ngay (tối đa ở
 *              main function kế tiếp tùy MDT/TMS).
 *            • Com_TriggerIPDUSend (8.3.3.19): trả Std_ReturnType (E_OK/E_NOT_OK).
 *
 * @version 1.2
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/

#include <stdint.h>
#include "Std_Types.h"   /* E_OK, E_NOT_OK, Std_ReturnType, boolean, uint8,... */
#include "Com.h"
#include "PduR_Com.h"
#include "Can.h"
#include <string.h>      /* memset */

/* ===== Kiểm tra cấu hình bắt buộc ===== */
#ifndef COM_MAX_IPDU_LEN
#error "COM_MAX_IPDU_LEN must be defined in Com_Cfg.h"
#endif

/* ===== Các bảng cấu hình ===== */
extern const Com_IPduCfgType   Com_IPduCfg[COM_NUM_IPDUS];
extern const Com_SignalCfgType Com_SignalCfg[COM_NUM_SIGNALS];

/* ===== Bù an toàn nếu header chưa định nghĩa mã SWS (giữ tính tương thích) ===== */
#ifndef COM_SERVICE_NOT_AVAILABLE
#define COM_SERVICE_NOT_AVAILABLE   ((uint8)0x80u)
#endif
#ifndef COM_BUSY
#define COM_BUSY                    ((uint8)0x81u)
#endif

/* ===== Shadow buffer cho từng I-PDU (TX) ===== */
static uint8 Com_IpduBuf[COM_NUM_IPDUS][COM_MAX_IPDU_LEN];  //0xc3   // 6 8   // 0 0 0 8 -> 0xca7, 1 0 - 1 8
/* ---------------------------------------------------------
 * Helpers nội bộ: tra cứu I-PDU & lấy buffer
 * ---------------------------------------------------------*/

/**
 * @brief  Tìm index I-PDU theo PduId trong bảng cấu hình
 * @param  pduId  ID I-PDU (ComConf_ComIPdu_*)
 * @return -1 nếu không thấy; ngược lại là index
 */
static int16_t prv_find_ipdu_index(PduIdType pduId)
{
    for (uint16 i = 0u; i < COM_NUM_IPDUS; ++i) {
        if (Com_IPduCfg[i].PduId == pduId) {
            return (int16_t)i;
        }
    }
    return (int16_t)-1;
}

/**
 * @brief  Lấy con trỏ buffer I-PDU kèm độ dài/hướng
 * @param  pduId   ID I-PDU
 * @param  outLen  [opt] độ dài I-PDU (byte)
 * @param  outDir  [opt] hướng I-PDU (TX/RX)
 * @return Con trỏ shadow buffer hoặc NULL nếu không hợp lệ
 */
static uint8* prv_get_ipdu_buf(PduIdType pduId,
                               PduLengthType* outLen,
                               Com_PduDirection_e* outDir)
{
    int16_t idx = prv_find_ipdu_index(pduId);
    if (idx < 0) {
        return NULL;
    }
    if (outLen) *outLen = Com_IPduCfg[(uint16)idx].Length;
    if (outDir) *outDir = Com_IPduCfg[(uint16)idx].direction;
    return &Com_IpduBuf[(uint16)idx][0];
}

/* ---------------------------------------------------------
 * Pack helpers (đặt giá trị vào byte/bit cụ thể)
 * ---------------------------------------------------------*/

/** Ghi 8-bit vào vị trí byteIndex của đích */
static inline void put_u8(uint8* dst, uint16 byteIndex, uint8 v)
{
    dst[byteIndex] = v;
}

/** Ghi 4-bit (nibble) vào [byteIndex : bitOffset..bitOffset+3] */
static void put_nibble(uint8* dst, uint16 byteIndex, uint8 bitOffset, uint8 v4)
{
    const uint8 mask = (uint8)(0x0Fu << bitOffset);
    const uint8 val  = (uint8)((v4 & 0x0Fu) << bitOffset);
    dst[byteIndex] = (uint8)((dst[byteIndex] & (uint8)(~mask)) | val);
}

/** Ghi 16-bit vào vị trí byteIndex (Big Endian giả định cho RPM) */
static inline void put_u16(uint8* dst, uint16 byteIndex, uint16 v)
{
    dst[byteIndex]     = (uint8)(v >> 8);
    dst[byteIndex + 1] = (uint8)(v & 0xFFu);
}

/** Ghi 1-bit vào [byteIndex : bitOffset] */
static void put_bit(uint8* dst, uint16 byteIndex, uint8 bitOffset, boolean b)
{
    const uint8 mask = (uint8)(1u << bitOffset);
    if (b) { dst[byteIndex] |=  mask; }
    else   { dst[byteIndex] &= (uint8)(~mask); }
}

/* ---------------------------------------------------------
 * Unpack helpers (lấy giá trị từ byte/bit cụ thể)
 * ---------------------------------------------------------*/

/** Đọc 8-bit từ vị trí byteIndex */
static inline uint8 get_u8(const uint8* src, uint16 byteIndex)
{
    return src[byteIndex];
}

/** Đọc 16-bit từ vị trí byteIndex (Big Endian giả định cho RPM) */
static inline uint16 get_u16(const uint8* src, uint16 byteIndex)
{
    return (uint16)(((uint16)src[byteIndex] << 8) | (uint16)src[byteIndex + 1]);
}

/** Đọc 1-bit từ [byteIndex : bitOffset] */
static inline boolean get_bit(const uint8* src, uint16 byteIndex, uint8 bitOffset)
{
    return (src[byteIndex] & (uint8)(1u << bitOffset)) ? TRUE : FALSE;
}

static inline uint8 get_nibble(const uint8* src, uint16 byteIndex, uint8 bitOffset)
{
    return (uint8)((src[byteIndex] >> bitOffset) & 0x0Fu);
}

__attribute__((weak)) void App_ComRxIndication(PduIdType ComRxPduId)
{
    (void)ComRxPduId;
}

/* =========================================================
 * Lifecycle
 * =======================================================*/

/**
 * @brief  Khởi tạo COM (shadow buffers/biến nội bộ)
 * @details Sau Com_Init, liên lạc inter-ECU vẫn chưa bật; cần start I-PDU groups
 *          theo cơ chế hệ thống nếu muốn truyền.
 */
void Com_Init(void)
{
    (void)memset(Com_IpduBuf, 0, sizeof(Com_IpduBuf));
}

/* =========================================================
 * TX API – bám kiểu trả về theo SWS
 * =======================================================*/

/**
 * @brief  Cập nhật giá trị signal vào shadow buffer của I-PDU
 * @param  SignalId      ID của signal (index vào Com_SignalCfg)
 * @param  SignalDataPtr Con trỏ dữ liệu nguồn
 * @return uint8  E_OK / COM_SERVICE_NOT_AVAILABLE / COM_BUSY
 * @note   Bản demo không có quản lý TP-buffer nên không phát sinh COM_BUSY.
 *         Các lỗi tham số/biên (ID không hợp lệ, hướng không đúng, vượt biên)
 *         được quy về COM_SERVICE_NOT_AVAILABLE để tuân kiểu trả về SWS.
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if (SignalDataPtr == NULL) {
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (SignalId >= (Com_SignalIdType)COM_NUM_SIGNALS) {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    const Com_SignalCfgType* cfg = &Com_SignalCfg[SignalId];

    PduLengthType ipduLen = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_RX; /* init để tránh cảnh báo */
    uint8* ipdu = prv_get_ipdu_buf(cfg->PduId, &ipduLen, &dir);
    if ((ipdu == NULL) || (dir != COM_PDU_DIR_TX) || (cfg->byteIndex >= ipduLen)) {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    /* Pack theo cấu hình độ dài/kiểu */
    switch (cfg->bitLength) {
        case 16u:
            put_u16(ipdu, cfg->byteIndex, *(const uint16*)SignalDataPtr);
            break;

        case 8u:
            if (cfg->type == COM_SIGTYPE_BOOLEAN) {
                const uint8 v = (*(const boolean*)SignalDataPtr) ? 1u : 0u;
                put_u8(ipdu, cfg->byteIndex, v);
            } else {
                put_u8(ipdu, cfg->byteIndex, *(const uint8*)SignalDataPtr);
            }
            break;

        case 4u: {
            const uint8 v4 = (uint8)(*(const uint8*)SignalDataPtr & 0x0Fu);
            put_nibble(ipdu, cfg->byteIndex, cfg->bitOffset, v4);
        } break;

        case 1u: {
            const boolean b = (*(const boolean*)SignalDataPtr) ? TRUE : FALSE;
            put_bit(ipdu, cfg->byteIndex, cfg->bitOffset, b);
        } break;

        default:
            /* Ngoài phạm vi demo (>8 bit, endianness đặc biệt, dynamic length…) */
            return COM_SERVICE_NOT_AVAILABLE;
    }

    /* Gợi ý: nếu cấu hình TransferProperty=TRIGGERED và TxMode=DIRECT/MIXED,
       có thể gọi Com_TriggerIPDUSend(cfg->PduId) tại đây tùy MDT/TMS. */

    return E_OK;
}

/**
 * @brief  Kích phát gửi I-PDU qua PduR
 * @param  PduId ID I-PDU (TX)
 * @return Std_ReturnType  E_OK / E_NOT_OK
 * @details Thực thi đơn giản: kiểm tra buffer/hướng/độ dài rồi gọi PduR_ComTransmit().
 *          Muốn bám chuẩn hoàn toàn, cần kiểm tra trạng thái I-PDU group (started?).
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    PduLengthType len = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_RX;
    uint8* buf = prv_get_ipdu_buf(PduId, &len, &dir);

    if ((buf == NULL) || (dir != COM_PDU_DIR_TX) ||
        (len == 0u) || (len > (PduLengthType)COM_MAX_IPDU_LEN)) {
        return E_NOT_OK;
    }

    PduInfoType info;
    info.SduDataPtr = buf;
    info.SduLength  = len;

    /* Khai báo nguyên mẫu ở PduR.h; nếu chưa có, bạn có thể extern:
       extern Std_ReturnType PduR_ComTransmit(PduIdType, const PduInfoType*); */
    return PduR_ComTransmit(PduId, &info);
}

/**
 * @brief  Đọc giá trị signal từ shadow buffer của I-PDU
 * @param  SignalId      ID của signal
 * @param  SignalDataPtr Con trỏ đích để lưu dữ liệu
 * @return uint8  E_OK / COM_SERVICE_NOT_AVAILABLE
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if (SignalDataPtr == NULL) {
        return COM_SERVICE_NOT_AVAILABLE;
    }
    if (SignalId >= (Com_SignalIdType)COM_NUM_SIGNALS) {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    const Com_SignalCfgType* cfg = &Com_SignalCfg[SignalId];

    PduLengthType ipduLen = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_TX;
    uint8* ipdu = prv_get_ipdu_buf(cfg->PduId, &ipduLen, &dir);

    if ((ipdu == NULL) || (dir != COM_PDU_DIR_RX) || (cfg->byteIndex >= ipduLen)) {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    /* Unpack theo cấu hình độ dài */
    switch (cfg->bitLength) {
        case 16u:
            *(uint16*)SignalDataPtr = get_u16(ipdu, cfg->byteIndex);
            break;

        case 8u:
            if (cfg->type == COM_SIGTYPE_BOOLEAN) {
                *(boolean*)SignalDataPtr = (get_u8(ipdu, cfg->byteIndex) != 0u) ? TRUE : FALSE;
            } else {
                *(uint8*)SignalDataPtr = get_u8(ipdu, cfg->byteIndex);
            }
            break;

        case 1u:
            *(boolean*)SignalDataPtr = get_bit(ipdu, cfg->byteIndex, cfg->bitOffset);
            break;

        case 4u:
            *(uint8*)SignalDataPtr = get_nibble(ipdu, cfg->byteIndex, cfg->bitOffset);
            break;

        default:
            return COM_SERVICE_NOT_AVAILABLE;
    }

    return E_OK;
}

/* =========================================================
 * Callback/Callout cho PduR/CanIf (tối thiểu để link)
 * =======================================================*/

void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) {
        return;
    }

    PduLengthType len = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_TX;
    uint8* buf = prv_get_ipdu_buf(ComRxPduId, &len, &dir);

    if ((buf != NULL) && (dir == COM_PDU_DIR_RX)) {
        /* Copy data từ Bus vào shadow buffer.
           Bản demo chỉ copy tối đa theo độ dài cấu hình của PDU. */
        PduLengthType copyLen = (PduInfoPtr->SduLength < len) ? PduInfoPtr->SduLength : len;
        (void)memcpy(buf, PduInfoPtr->SduDataPtr, copyLen);
        App_ComRxIndication(ComRxPduId);
    }
}

void Com_TxConfirmation(PduIdType ComTxPduId)
{
    (void)ComTxPduId;
    /* Có thể cập nhật trạng thái I-PDU ở đây nếu cần */
}

Std_ReturnType Com_TriggerTransmit(PduIdType ComTxPduId, PduInfoType* PduInfoPtr)
{
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) {
        return E_NOT_OK;
    }

    PduLengthType len = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_RX;
    uint8* buf = prv_get_ipdu_buf(ComTxPduId, &len, &dir);
    if ((buf == NULL) || (dir != COM_PDU_DIR_TX) || (len == 0u) || (len > COM_MAX_IPDU_LEN)) {
        return E_NOT_OK;
    }

    (void)memcpy(PduInfoPtr->SduDataPtr, buf, len);
    PduInfoPtr->SduLength = len;
    return E_OK;
}
