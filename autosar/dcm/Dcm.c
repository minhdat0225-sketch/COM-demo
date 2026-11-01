#include "Dcm.h"
#include <stdio.h>
#include <string.h>

/* Khai báo hàm in log từ hệ thống hiện hành */
extern void Log_Print(const char* str);

/* Bộ đệm nhận dữ liệu chẩn đoán nội bộ của DCM */
#define DCM_RX_BUFFER_SIZE 256
static uint8 Dcm_RxBuffer[DCM_RX_BUFFER_SIZE];
static PduLengthType Dcm_RxLength = 0;
static boolean Dcm_IsReceiving = FALSE;

/**
 * @brief Khởi tạo module DCM
 */
void Dcm_Init(void) {
    Dcm_RxLength = 0;
    Dcm_IsReceiving = FALSE;
    memset(Dcm_RxBuffer, 0, DCM_RX_BUFFER_SIZE);
    Log_Print("[DCM] Initialized successfully.\r\n");
}

/**
 * @brief Bắt đầu quá trình nhận Diagnostic PDU (gọi khi có First Frame hoặc Single Frame)
 */
BufReq_ReturnType Dcm_StartOfReception(
    PduIdType id,
    const PduInfoType* info,
    PduLengthType TpSduLength,
    PduLengthType* bufferSizePtr) 
{
    (void)id;
    (void)info;
    
    /* Kiểm tra xem dung lượng yêu cầu có vượt quá bộ đệm của DCM không */
    if (TpSduLength > DCM_RX_BUFFER_SIZE) {
        Log_Print("[DCM] Error: Requested PDU length exceeds buffer size!\r\n");
        return BUFREQ_E_OVFL;
    }
    
    Dcm_IsReceiving = TRUE;
    Dcm_RxLength = 0; /* Reset chiều dài nhận */
    memset(Dcm_RxBuffer, 0, DCM_RX_BUFFER_SIZE);
    
    /* Cấp phát bộ đệm cho tầng dưới biết */
    if (bufferSizePtr != NULL) {
        *bufferSizePtr = DCM_RX_BUFFER_SIZE;
    }
    return BUFREQ_OK;
}

/**
 * @brief Sao chép dữ liệu từ tầng dưới (CanTp) lên bộ đệm DCM
 */
BufReq_ReturnType Dcm_CopyRxData(
    PduIdType id,
    const PduInfoType* info,
    PduLengthType* bufferSizePtr) 
{
    (void)id;
    
    if (!Dcm_IsReceiving || info == NULL || info->SduDataPtr == NULL) {
        return BUFREQ_E_NOT_OK;
    }
    
    if (Dcm_RxLength + info->SduLength > DCM_RX_BUFFER_SIZE) {
        return BUFREQ_E_OVFL;
    }
    
    /* Sao chép từng mảnh dữ liệu (Data Payload) vào buffer tĩnh của DCM */
    memcpy(&Dcm_RxBuffer[Dcm_RxLength], info->SduDataPtr, info->SduLength);
    Dcm_RxLength += info->SduLength;
    
    /* Cập nhật báo cáo sức chứa còn lại của bộ đệm */
    if (bufferSizePtr != NULL) {
        *bufferSizePtr = DCM_RX_BUFFER_SIZE - Dcm_RxLength;
    }
    
    return BUFREQ_OK;
}

/**
 * @brief Quá trình nhận dữ liệu kết thúc (Gói hoàn chỉnh hoặc bị lỗi ngang)
 */
void Dcm_TpRxIndication(PduIdType id, NotifResultType result) 
{
    (void)id;
    char msg[128];
    
    if (result == NTFRSLT_OK) {
        /* Phân tích Service ID chẩn đoán */
        uint8 serviceId = Dcm_RxBuffer[0];
        sprintf(msg, "[DCM] Nhận thành công UDS Message! Length=%u bytes, ServiceID=0x%02X\r\n", Dcm_RxLength, serviceId);
        Log_Print(msg);
        
        /* Hiển thị payload thực tế */
        Log_Print("[DCM] Payload Hex: ");
        for (PduLengthType i = 0; i < Dcm_RxLength && i < 16; i++) {
            sprintf(msg, "%02X ", Dcm_RxBuffer[i]);
            Log_Print(msg);
        }
        if (Dcm_RxLength > 16) Log_Print("...");
        Log_Print("\r\n");
    } else {
        sprintf(msg, "[DCM] Lỗi quá trình nhận chẩn đoán! Result Code: %d\r\n", result);
        Log_Print(msg);
    }
    
    Dcm_IsReceiving = FALSE;
}
