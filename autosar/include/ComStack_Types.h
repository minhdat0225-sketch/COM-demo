/**********************************************************
 * @file    ComStack_Types.h
 * @brief   Kiểu dữ liệu chung cho Communication Stack (AUTOSAR)
 *
 * @details File này định nghĩa các kiểu dữ liệu dùng chung
 *          giữa tất cả các module trong COM stack:
 *          COM, PduR, CanIf, LinIf, CAN Driver, LIN Driver.
 *
 *          Các kiểu chính:
 *          ┌─────────────────────────────────────────────┐
 *          │ PduIdType      │ ID duy nhất của mỗi I-PDU  │
 *          │ PduLengthType  │ Độ dài payload (byte)      │
 *          │ PduInfoType    │ Mô tả SDU (data + length)  │
 *          │ BufReq_ReturnType │ Kết quả cấp buffer (TP) │
 *          │ NotifResultType   │ Kết quả thông báo       │
 *          └─────────────────────────────────────────────┘
 *
 *          Tham chiếu: AUTOSAR_SWS_CommunicationStackTypes (R4.3+)
 *
 * @version 1.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

/* ===========================================================
 * PduIdType – ID định danh I-PDU
 * -----------------------------------------------------------
 * Mỗi I-PDU (Interaction Layer Protocol Data Unit) trong hệ
 * thống có một ID duy nhất. ID này được dùng xuyên suốt các
 * module: COM gán signal vào I-PDU theo ID, PduR tra bảng
 * route theo ID, CanIf/LinIf map ID sang CAN-ID/LIN-ID.
 *
 * Kiểu uint16: hỗ trợ tới 65535 I-PDU trong hệ thống.
 * ===========================================================*/
typedef uint16  PduIdType;

/* ===========================================================
 * PduLengthType – Độ dài payload của I-PDU
 * -----------------------------------------------------------
 * Đơn vị: byte. Với CAN Classic tối đa 8 byte,
 * CAN FD tối đa 64 byte, LIN tối đa 8 byte.
 * ===========================================================*/
typedef uint16  PduLengthType;

/* ===========================================================
 * Kiểu chỉ số mạng/PNC
 * -----------------------------------------------------------
 * NetworkHandleType: chỉ số kênh mạng (CAN, LIN, ETH...)
 * PNCHandleType: chỉ số Partial Networking Cluster
 * ===========================================================*/
typedef uint8   NetworkHandleType;
typedef uint8   PNCHandleType;

/* ===========================================================
 * TpDataStateType – Trạng thái dữ liệu Transport Protocol
 * -----------------------------------------------------------
 * Dùng trong giao thức TP (Transport Protocol) để chỉ trạng
 * thái truyền dữ liệu phân đoạn (segmented transfer).
 *
 * TP_DATACONF   : Dữ liệu đã được xác nhận gửi thành công
 * TP_DATARETRY  : Cần gửi lại một phần dữ liệu
 * TP_CONFPENDING: Đang chờ xác nhận
 * ===========================================================*/
typedef enum {
    TP_DATACONF    = 0,
    TP_DATARETRY   = 1,
    TP_CONFPENDING = 2
} TpDataStateType;

/* ===========================================================
 * RetryInfoType – Thông tin retry cho TP
 * -----------------------------------------------------------
 * PduR dùng cấu trúc này khi gọi lên upper-layer để thông
 * báo cần gửi lại dữ liệu.
 *
 * TpDataState : Trạng thái TP hiện tại
 * TxTpDataCnt : Số byte còn lại cần gửi lại
 * ===========================================================*/
typedef struct {
    TpDataStateType TpDataState;
    PduLengthType   TxTpDataCnt;
} RetryInfoType;

/* ===========================================================
 * NotifResultType – Kết quả thông báo cho upper layers
 * -----------------------------------------------------------
 * Các module TP/DoIP dùng kiểu này để báo kết quả truyền/nhận
 * lên tầng trên (COM, DCM...).
 *
 * NTFRSLT_OK          : Thành công
 * NTFRSLT_E_NOT_OK    : Thất bại chung
 * NTFRSLT_E_TIMEOUT_A : Hết thời gian chờ N_As
 * NTFRSLT_E_TIMEOUT_BS: Hết thời gian chờ N_Bs
 * ...
 * ===========================================================*/
typedef enum {
    NTFRSLT_OK             = 0x00,  /**< Thành công                   */
    NTFRSLT_E_NOT_OK       = 0x01,  /**< Thất bại chung               */
    NTFRSLT_E_CANCELATION  = 0x02,  /**< Đã bị hủy                    */
    NTFRSLT_E_TIMEOUT_A    = 0x03,  /**< Timeout N_As                  */
    NTFRSLT_E_TIMEOUT_BS   = 0x04,  /**< Timeout N_Bs                  */
    NTFRSLT_E_TIMEOUT_CR   = 0x05,  /**< Timeout N_Cr                  */
    NTFRSLT_E_WRONG_SN     = 0x06,  /**< Sai Sequence Number           */
    NTFRSLT_E_INVALID_FS   = 0x07,  /**< Flow Status không hợp lệ     */
    NTFRSLT_E_UNEXP_PDU    = 0x08,  /**< PDU không mong đợi           */
    NTFRSLT_E_WFT_OVRN     = 0x09,  /**< Wait Frame Transmit tràn     */
    NTFRSLT_E_NO_BUFFER    = 0x0A   /**< Không có buffer khả dụng     */
} NotifResultType;

/* ===========================================================
 * BufReq_ReturnType – Kết quả yêu cầu cấp phát buffer
 * -----------------------------------------------------------
 * Dùng bởi PduR/TP khi cần buffer từ upper/lower layer.
 *
 * BUFREQ_OK       : Có buffer, sẵn sàng
 * BUFREQ_E_NOT_OK : Lỗi chung
 * BUFREQ_E_BUSY   : Tạm thời bận (thử lại sau)
 * BUFREQ_E_OVFL   : Tràn buffer (dữ liệu quá lớn)
 * ===========================================================*/
typedef enum {
    BUFREQ_OK       = 0,
    BUFREQ_E_NOT_OK = 1,
    BUFREQ_E_BUSY   = 2,
    BUFREQ_E_OVFL   = 3
} BufReq_ReturnType;

/* ===========================================================
 * PduInfoType – Mô tả Service Data Unit (SDU)
 * -----------------------------------------------------------
 * Cấu trúc chính để truyền dữ liệu giữa các tầng trong
 * COM stack. Mỗi lần gọi API truyền/nhận, dữ liệu được
 * đóng gói trong PduInfoType:
 *
 * SduDataPtr  : Con trỏ tới mảng byte payload
 * MetaDataPtr : Con trỏ metadata (dùng cho SoAd/ETH, NULL nếu không dùng)
 * SduLength   : Chiều dài payload (byte)
 *
 * Ví dụ: Khi COM gọi PduR_ComTransmit():
 *   PduInfoType info;
 *   info.SduDataPtr = &buffer[0];  // Dữ liệu CAN/LIN
 *   info.SduLength  = 5;           // 5 byte
 *   info.MetaDataPtr = NULL;       // Không dùng metadata
 * ===========================================================*/
typedef struct {
    uint8*         SduDataPtr;   /**< Con trỏ tới payload (mảng byte)          */
    uint8*         MetaDataPtr;  /**< Con trỏ metadata (NULL nếu không dùng)   */
    PduLengthType  SduLength;    /**< Chiều dài payload (byte)                 */
} PduInfoType;

#ifdef __cplusplus
}
#endif

#endif /* COMSTACK_TYPES_H */
