#ifndef DCM_H
#define DCM_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* Khởi tạo module DCM */
void Dcm_Init(void);

/* Được gọi bởi PduR / CanTp khi bắt đầu nhận một chuỗi multi-frame (First Frame) hoặc SF */
BufReq_ReturnType Dcm_StartOfReception(
    PduIdType id,
    const PduInfoType* info,
    PduLengthType TpSduLength,
    PduLengthType* bufferSizePtr
);

/* Được gọi bởi PduR / CanTp để copy dữ liệu payload vừa nhận lên DCM */
BufReq_ReturnType Dcm_CopyRxData(
    PduIdType id,
    const PduInfoType* info,
    PduLengthType* bufferSizePtr
);

/* Được gọi bởi PduR / CanTp khi toàn bộ quá trình nhận dữ liệu đã hoàn tất */
void Dcm_TpRxIndication(PduIdType id, NotifResultType result);

#endif /* DCM_H */
