/**********************************************************
 * @file    LinIf.c
 * @brief   AUTOSAR LIN Interface (LinIf) – KÉO (PULL) DATA
 *
 * @details Tầng ECU Abstraction kết nối PduR (phía trên) với
 *          LIN Driver (phía dưới) hoạt động theo Schedule Table.
 *
 *          Kiến trúc KÉO (Pull Architecture chuẩn AUTOSAR PduR <-> LinIf):
 *          1. LinIf_MainFunction lặp liên tục, sử dụng biến tick đếm chu kỳ.
 *          2. Khi đến thời điểm phát Unconditional Frame (nhìn qua Schedule Table fake):
 *             - Gọi PduR_LinIfTriggerTransmit(...) để lôi dữ liệu Data mới nhất từ COM.
 *             - Đóng gói PID, Cs, DLC, SduPtr vào biến Lin_PduType.
 *             - Gọi Lin_SendFrame() xuống LIN Driver.
 *          3. Sang chu kỳ kế tiếp, kiểm tra Lin_GetStatus() == LIN_TX_OK.
 *             - Gọi PduR_LinIfTxConfirmation() ngược lên COM.
 *
 * @version 3.0 (Pulled)
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/

#include "LinIf.h"
#include "Lin.h"               /* Lin_SendFrame(), Lin_GetStatus() */
#include "PduR_LinIf.h"        /* API kéo data và confirm */
#include <stddef.h>
#include <string.h>

/* ===========================================================
 * Hằng số cấu hình
 * ===========================================================*/
#define LINIF_NUM_PDUS       3u   /**< Số lượng PDU: LightCtrl (TX), HVACCtrl (TX), TempSensor (RX) */
#define LINIF_CHANNEL        0u   /**< Kênh LIN duy nhất trong demo          */
#define LINIF_INVALID_PDU    255u /**< PDU không hợp lệ (Dùng cho trạng thái pending) */

/* ===========================================================
 * LinIf_PduCfgType – Bảng cấu hình đặc tính PDU của LinIf
 * ===========================================================*/
typedef struct {
    PduIdType              LinIfPduId;    /**< PDU ID phía LinIf */
    uint8                  LinFrameId;    /**< LIN frame ID (Pid) chuẩn cho frame       */
    uint8                  Dlc;           /**< Data Length chuẩn của frame               */
    Lin_FrameResponseType  Drc;           /**< Hướng dữ liệu: TX hoặc RX                */
} LinIf_PduCfgType;

/* Bảng cấu hình tĩnh */
static const LinIf_PduCfgType LinIf_PduCfg[LINIF_NUM_PDUS] = {
    { .LinIfPduId = 0u, .LinFrameId = 0x10u, .Dlc = 4u, .Drc = LIN_FRAMERESPONSE_TX },  /* LightCtrl */
    { .LinIfPduId = 1u, .LinFrameId = 0x11u, .Dlc = 3u, .Drc = LIN_FRAMERESPONSE_TX },  /* HVACCtrl  */
    { .LinIfPduId = 2u, .LinFrameId = 0x20u, .Dlc = 2u, .Drc = LIN_FRAMERESPONSE_RX }   /* TempSensor */
};

/* ===========================================================
 * Biến trạng thái nội bộ
 * ===========================================================*/
static boolean s_linIfInited = FALSE;             /**< Cờ đã khởi tạo */
static PduIdType s_pendingConf = LINIF_INVALID_PDU; /**< Theo dõi slot ID đang chờ Confirm/Indication của phần cứng */

/* Bộ đệm cục bộ chứa data kéo từ tầng trên xuống */
static uint8 s_local_tx_buffer[8];

/* ===========================================================
 * LinIf_Init – Khởi tạo
 * ===========================================================*/
void LinIf_Init(void)
{
    /* Báo cho MCAL khởi động thanh ghi / Pinout */
    Lin_Init();
    s_pendingConf = LINIF_INVALID_PDU;
    s_linIfInited = TRUE;
}

/* ===========================================================
 * LinIf_Transmit
 * ===========================================================*/
Std_ReturnType LinIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    (void)TxPduId;
    (void)PduInfoPtr;
    return E_NOT_OK; 
}

/* ===========================================================
 * LinIf_MainFunction – Bộ Não Lập Lịch của mạng LIN
 * ===========================================================*/
void LinIf_MainFunction(void)
{
    if (!s_linIfInited) return;

    /* 1. Xử Lý Confirmation / Indication cho chu kỳ gửi đợt trước */
    if (s_pendingConf != LINIF_INVALID_PDU) {
        uint8* rxBufPtr = NULL;
        Lin_StatusType st = Lin_GetStatus(LINIF_CHANNEL, &rxBufPtr);
        if (st == LIN_TX_OK) {
            /* Hardware báo xong -> Xác định hướng truyền */
            if (LinIf_PduCfg[s_pendingConf].Drc == LIN_FRAMERESPONSE_RX) {
                /* Gửi tín hiệu RX Indication lên PduR */
                PduInfoType rxPduInfo;
                rxPduInfo.SduDataPtr = rxBufPtr;
                rxPduInfo.SduLength  = LinIf_PduCfg[s_pendingConf].Dlc;
                PduR_LinIfRxIndication(LinIf_PduCfg[s_pendingConf].LinIfPduId, &rxPduInfo);
            } else {
                /* Báo TX Confirmation từ LinIf lên PduR */
                PduR_LinIfTxConfirmation(s_pendingConf);
            }
            s_pendingConf = LINIF_INVALID_PDU;
        } else if (st == LIN_TX_ERROR || st == LIN_OK) {
            /* Lỗi hoặc trạng thái trôi rỗng -> reset cờ */
            s_pendingConf = LINIF_INVALID_PDU;
        }
        /* Nếu BUSY, tiếp tục chờ */
        return; 
    }

    /* 2. Giả lập Lịch trình truyền LIN (Schedule Table) */
    static uint32 tick = 0;
    tick++;
    
    PduIdType slotId = LINIF_INVALID_PDU;
    
    /* Gửi slot 0: LightCtrl vào tick % 10 == 0 */
    if (tick % 10 == 0) {
        slotId = 0u; 
    }
    /* Gửi slot 1: HVACCtrl vào tick % 10 == 5 */
    else if (tick % 10 == 5) {
        slotId = 1u;
    }
    /* Gửi slot 2: TempSensor vào tick % 10 == 8 */
    else if (tick % 10 == 8) {
        slotId = 2u;
    }

    /* 3. Lệnh truyền thực tế nếu Trúng slot */
    if (slotId != LINIF_INVALID_PDU) {
        extern void Log_Print(const char* str);
        
        if (LinIf_PduCfg[slotId].Drc == LIN_FRAMERESPONSE_TX) {
            PduInfoType pduInfo;
            pduInfo.SduDataPtr = s_local_tx_buffer;
            pduInfo.SduLength  = LinIf_PduCfg[slotId].Dlc;
            
            /* Kéo (pull) dữ liệu từ COM qua PduR */
            Std_ReturnType ret = PduR_LinIfTriggerTransmit(LinIf_PduCfg[slotId].LinIfPduId, &pduInfo);
            
            if (ret == E_OK) {
                Log_Print("[LinIf] PduR returned E_OK!\r\n");
                Lin_PduType linPdu;
                linPdu.Pid    = LinIf_PduCfg[slotId].LinFrameId; 
                linPdu.Cs     = LIN_ENHANCED_CS;
                linPdu.Drc    = LIN_FRAMERESPONSE_TX;
                linPdu.Dl     = pduInfo.SduLength;
                linPdu.SduPtr = pduInfo.SduDataPtr;

                if (Lin_SendFrame(LINIF_CHANNEL, &linPdu) == E_OK) {
                    Log_Print("[LinIf] Lin_SendFrame SUCCESS (TX)!\r\n");
                    char hex[16];
                    for(int i = 0; i < linPdu.Dl; i++) {
                        hex[0] = '0';
                        hex[1] = 'x';
                        hex[2] = "0123456789ABCDEF"[linPdu.SduPtr[i] >> 4];
                        hex[3] = "0123456789ABCDEF"[linPdu.SduPtr[i] & 0x0F];
                        hex[4] = ' '; hex[5] = '\0';
                        Log_Print(hex);
                    }
                    Log_Print("\r\n");
                    s_pendingConf = slotId;
                } else {
                    Log_Print("[LinIf] Lin_SendFrame FAILED!\r\n");
                }
            }
        } else if (LinIf_PduCfg[slotId].Drc == LIN_FRAMERESPONSE_RX) {
            /* Đối với RX, Master phát Header, không pull từ COM */
            Lin_PduType linPdu;
            linPdu.Pid    = LinIf_PduCfg[slotId].LinFrameId; 
            linPdu.Cs     = LIN_ENHANCED_CS;
            linPdu.Drc    = LIN_FRAMERESPONSE_RX;
            linPdu.Dl     = LinIf_PduCfg[slotId].Dlc;
            linPdu.SduPtr = s_local_tx_buffer; /* Không sử dụng trong RX để truyền nhưng cấp để tránh NULL */

            Log_Print("[LinIf] Sending LIN RX Header Request...\r\n");
            if (Lin_SendFrame(LINIF_CHANNEL, &linPdu) == E_OK) {
                Log_Print("[LinIf] Lin_SendFrame SUCCESS (RX Header)!\r\n");
                s_pendingConf = slotId;
            } else {
                Log_Print("[LinIf] Lin_SendFrame RX FAILED!\r\n");
            }
        }
    }
}

/* ===========================================================
 * LinIf_GotoSleep
 * ===========================================================*/
Std_ReturnType LinIf_GotoSleep(uint8 Channel)
{
    if (!s_linIfInited) return E_NOT_OK;
    return Lin_GoToSleep(Channel);
}

/* ===========================================================
 * LinIf_Wakeup
 * ===========================================================*/
Std_ReturnType LinIf_Wakeup(uint8 Channel)
{
    if (!s_linIfInited) return E_NOT_OK;
    return Lin_Wakeup(Channel);
}
