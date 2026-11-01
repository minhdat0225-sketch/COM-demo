# Kịch Bản Tích Hợp & Luồng Xử Lý Đồng Bộ COM Stack

Tài liệu này trình bày chi tiết và hệ thống hóa 3 kịch bản truyền/nhận dữ liệu thực tế chạy trong project. Mỗi kịch bản mô tả dòng chảy dữ liệu đồng bộ xuyên suốt từ các tầng trên cùng (**App/COM/Dcm**) đi qua bộ định tuyến **PduR**, tầng vận chuyển **CanTp**, tầng trừu tượng **CanIf**, xuống Driver phần cứng **Can (MCAL)** và luồng phản hồi/chuyển tiếp ngược lại từ dưới lên trên.

---

## 1. Bản Đồ Tổng Quan Sự Tương Tác Giữa Các Lớp (Call Stack Map)

```
  TẦNG DỊCH VỤ (Service Layer)
    [COM]                     [DCM]
      │                         │
      │ PduR_ComTransmit()      │ CanTp_Transmit() / Dcm_StartOfReception()
      ▼                         ▼
  TẦNG ĐỊNH TUYẾN (Routing Layer)
    [PduR] <─────────────────> [PduR_CanTp...]
      │ PduR_ComTransmit()      │
      ▼ (Định tuyến trực tiếp)  │
  TẦNG VẬN CHUYỂN (Transport Layer)
      │                         │ PduR_CanTpCopyRxData() / PduR_CanTpRxIndication()
      │                         ▼
      │                      [CanTp]
      │                         │ CanIf_Transmit() (FF, CF, FC)
      │                         ▼
  TẦNG TRỪU TƯỢNG PHẦN CỨNG (ECU Abstraction Layer)
    [CanIf] <───────────────────┘
      │ Can_Write()
      ▼
  TẦNG MCAL DRIVER (Microcontroller Abstraction Layer)
    [Can Driver] (bxCAN)
```

---

## 2. Kịch Bản 1: Truyền Tín Hiệu CAN Cổ Điển Nhỏ Từ COM (Luồng TX)
*Kịch bản này mô tả quá trình gửi tín hiệu điều khiển động cơ `EngineCmd` (CAN ID `0x180`, độ dài 5 bytes) từ ứng dụng đi xuống và quá trình nhận xác nhận.*

```mermaid
sequenceDiagram
    autonumber
    participant App as 1. Application (main.c)
    participant COM as 2. COM (Com.c)
    participant PduR as 3. PduR (PduR.c)
    participant CanIf as 4. CanIf (CanIf.c)
    participant Can as 5. Can Driver (bxCAN)

    Note over App, COM: Bước 1: Cập nhật & Gửi tín hiệu điều khiển
    App ->> COM: Com_SendSignal(ComSig_Engine_Throttle, &throttle)
    App ->> COM: Com_TriggerIPDUSend(ComConf_ComIPdu_EngineCmd)
    Note over COM: Gói các tín hiệu vào PduBuffer<br/>SduLength = 5 bytes

    Note over COM, PduR: Bước 2: Định tuyến PDU
    COM ->> PduR: PduR_ComTransmit(ComConf_ComIPdu_EngineCmd, &PduInfo)
    Note over PduR: Tra bảng PduR_ComTxRoutes<br/>Tìm thấy: DestModule = PDUR_DEST_CANIF

    Note over PduR, CanIf: Bước 3: Trừu tượng hóa CAN
    PduR ->> CanIf: CanIf_Transmit(CanIfConf_Pdu_EngineCmd, &PduInfo)
    Note over CanIf: Tra bảng CanIf_TxPduCfg<br/>Map sang CAN ID = 0x180, Hth = 0

    Note over CanIf, Can: Bước 4: Driver ghi phần cứng
    CanIf ->> Can: Can_Write(0, &Can_PduType)
    Note over Can: Đẩy dữ liệu vào Tx Mailbox phần cứng<br/>Trả về CAN_OK
    Can -->> CanIf: CAN_OK
    CanIf -->> PduR: E_OK
    PduR -->> COM: E_OK

    Note over Can, App: Bước 5: Luồng xác nhận truyền (TxConfirmation)
    Can ->> CanIf: CanIf_TxConfirmation(CanIfConf_Pdu_EngineCmd)
    CanIf ->> PduR: PduR_CanIfTxConfirmation(CanIfConf_Pdu_EngineCmd)
    Note over PduR: Tra bảng PduR_CanIfTxConfRoutes
    PduR ->> COM: Com_TxConfirmation(ComConf_ComIPdu_EngineCmd)
    Note over COM: Giải phóng trạng thái gửi tín hiệu
```

### Chi tiết hành vi API đồng bộ (Kịch bản 1):
1.  **`Com_SendSignal`**: Ghi giá trị `throttle` mới vào Shadow Buffer nội bộ của COM tại tín hiệu [ComSig_Engine_Throttle](file:///Users/phamvanvu/HALAAcademy/01_Courses/Autosar%20Classic/COM/config/Com_Cfg.h).
2.  **`Com_TriggerIPDUSend`**: Bắt đầu đóng gói toàn bộ các bit tín hiệu của PDU [ComConf_ComIPdu_EngineCmd](file:///Users/phamvanvu/HALAAcademy/01_Courses/Autosar%20Classic/COM/config/Com_Cfg.h) vào bộ đệm gửi tĩnh. Sau đó gọi hàm định tuyến:
    *   **Call Stack**: `Com_TriggerIPDUSend` -> `PduR_ComTransmit(0, &info)`.
3.  **`PduR_ComTransmit`**: Thực hiện tìm kiếm tuyến trong bảng tĩnh `PduR_ComTxRoutes`. Định tuyến phát hiện Destination là `PDUR_DEST_CANIF`.
    *   **Call Stack**: `PduR_ComTransmit` -> `CanIf_Transmit(CanIfConf_Pdu_EngineCmd, PduInfoPtr)`.
4.  **`CanIf_Transmit`**: Tìm kiếm cấu hình PDU trong `CanIf_TxPduCfg`. Thiết lập cấu trúc Driver `Can_PduType` (chứa `id=0x180`, `length=5`, con trỏ dữ liệu).
    *   **Call Stack**: `CanIf_Transmit` -> `Can_Write(0, &frame)`.
5.  **`Can_Write`**: Thực hiện ghi vào thanh ghi bộ đệm truyền phần cứng (bxCAN). Trả về `CAN_OK`.
6.  **`Can_MainFunction_Write`**: Chạy ngắt/polling kiểm tra cờ truyền phần cứng xong. Gọi ngược lên:
    *   `CanIf_TxConfirmation(CanIfConf_Pdu_EngineCmd)` -> `PduR_CanIfTxConfirmation` -> `Com_TxConfirmation`.

---

## 3. Kịch Bản 2: Truyền Chẩn Đoán Dữ Liệu Lớn Phân Mảnh (UDS TX - Luồng CanTp)
*Kịch bản này mô tả ứng dụng gửi gói tin chẩn đoán UDS dài 20 bytes (kích thước lớn hơn 8 bytes tối đa của CAN Classic). Quá trình diễn ra đồng bộ qua cơ chế phân đoạn (First Frame -> Chờ nhận Flow Control -> Gửi Consecutive Frames).*

```mermaid
sequenceDiagram
    autonumber
    participant App as Application / DCM
    participant CanTp as CanTp Module
    participant CanIf as CanIf Module
    participant Can as Can Driver (bxCAN)

    Note over App, CanTp: Bước 1: Yêu cầu gửi gói tin lớn (20 bytes)
    App ->> CanTp: CanTp_Transmit(CanTpConf_CanTpTxNSdu_DiagTx, &tpInfo)
    Note over CanTp: Phát hiện chiều dài 20 > 7 bytes<br/>1. TotalLength=20, SentLength=6, SN=1<br/>2. Chuyển State sang CANTP_TX_WAIT_FC<br/>3. Tạo First Frame (FF)
    CanTp ->> CanIf: CanIf_Transmit(3, FF_Info)
    CanIf ->> Can: Can_Write(0, FF_Pdu)
    Note over CanTp: Cài đặt Timer_N_Bs

    Note over Can, CanTp: Bước 2: Nhận Flow Control (FC) từ đối tác
    Can ->> CanIf: CanIf_RxIndication(0x7E0, FC_Pdu)
    CanIf ->> CanTp: CanTp_RxIndication(0, FC_Pdu)
    Note over CanTp: Phát hiện PCI Type = 0x03 (FC)<br/>FlowStatus = 0 (CTS)<br/>1. State = CANTP_TX_WAIT_CF_CONF<br/>2. Tạo Consecutive Frame đầu (CF1, SN=1)
    CanTp ->> CanIf: CanIf_Transmit(3, CF1_Info)
    CanIf ->> Can: Can_Write(0, CF1_Pdu)
    Note over CanTp: SentLength = 6 + 7 = 13 bytes. SN = 2

    Note over Can, CanTp: Bước 3: Xác nhận truyền xong CF1 & Gửi CF2
    Can ->> CanIf: CanIf_TxConfirmation(3)
    CanIf ->> CanTp: CanTp_TxConfirmation(3)
    Note over CanTp: Phát hiện SentLength (13) < TotalLength (20)<br/>Chuyển State = CANTP_TX_SEND_CF
    
    rect rgb(245, 245, 245)
        Note over CanTp: Tác vụ tuần kỳ phát hiện State == CANTP_TX_SEND_CF
        Note over CanTp: CanTp_MainFunction()
        Note over CanTp: 1. Tạo CF2 (SN=2, mang 7 bytes)<br/>2. State = CANTP_TX_WAIT_CF_CONF
        CanTp ->> CanIf: CanIf_Transmit(3, CF2_Info)
        CanIf ->> Can: Can_Write(0, CF2_Pdu)
        Note over CanTp: SentLength = 13 + 7 = 20 bytes. SN = 3
    end

    Note over Can, CanTp: Bước 4: Xác nhận truyền xong CF2 & Gửi CF3 (Cuối)
    Can ->> CanIf: CanIf_TxConfirmation(3)
    CanIf ->> CanTp: CanTp_TxConfirmation(3)
    Note over CanTp: Phát hiện SentLength (20) < TotalLength (20) -> Sai (Đã bằng)<br/>Thực tế: Hàm phát hiện đã gửi đủ trong chu kỳ quét kế tiếp
    
    rect rgb(245, 245, 245)
        Note over CanTp: CanTp_MainFunction()
        Note over CanTp: State == CANTP_TX_SEND_CF<br/>1. Tạo CF3 (SN=3, mang 6 bytes còn lại)<br/>2. Đổi State = CANTP_TX_WAIT_CF_CONF
        CanTp ->> CanIf: CanIf_Transmit(3, CF3_Info)
        CanIf ->> Can: Can_Write(0, CF3_Pdu)
        Note over CanTp: Thông báo truyền hoàn tất lên PduR
        CanTp ->> App: PduR_CanTpTxConfirmation(..., NTFRSLT_OK)
    end
    
    Can ->> CanIf: CanIf_TxConfirmation(3)
    CanIf ->> CanTp: CanTp_TxConfirmation(3)
    Note over CanTp: SentLength >= TotalLength<br/>Chuyển State về CANTP_IDLE
```

### Chi tiết hành vi API đồng bộ (Kịch bản 2):
1.  **`CanTp_Transmit`**:
    *   Hàm nhận vào ID cấu hình `CanTpConf_CanTpTxNSdu_DiagTx` (Index 0).
    *   Copy 20 bytes dữ liệu chẩn đoán vào buffer tĩnh `TxState[0].TxBuffer`.
    *   Dựng PCI First Frame: `Byte 0 = 0x10`, `Byte 1 = 0x14` (Hex của 20).
    *   Gửi 6 bytes đầu tiên của `DiagTx` qua `CanIf_Transmit(3, &canIfTxInfo)` (Index 3 là ID CAN `0x7E0` của chẩn đoán).
    *   Đặt `TxState[0].State = CANTP_TX_WAIT_FC`.
2.  **`CanTp_RxIndication` (Nhận Flow Control)**:
    *   Được gọi khi bên nhận phản hồi frame FC (CAN ID `0x7E0`).
    *   Phân tích byte PCI đầu tiên `0x30` -> CTS (Clear to Send).
    *   Đổi trạng thái: `TxState[0].State = CANTP_TX_WAIT_CF_CONF`.
    *   Gửi trực tiếp CF1 (SN=1) mang 7 bytes tiếp theo (Tổng bytes đã gửi = 13).
3.  **`CanTp_TxConfirmation` & `CanTp_MainFunction` (Pacing gửi CF)**:
    *   Mỗi khi Driver hoàn thành truyền 1 frame trên bus, callback `CanTp_TxConfirmation` đổi trạng thái kênh truyền sang `CANTP_TX_SEND_CF`.
    *   `CanTp_MainFunction` chạy ngầm định kỳ quét và phát hiện trạng thái `CANTP_TX_SEND_CF`, thực hiện gửi CF tiếp theo (CF2, CF3) và đưa trạng thái về `CANTP_TX_WAIT_CF_CONF` để chờ xác nhận tiếp theo từ phần cứng.
    *   Khi byte cuối cùng được truyền đi, báo cáo thành công lên `PduR` và giải phóng máy trạng thái về `CANTP_IDLE`.

---

## 4. Kịch Bản 3: Nhận Dữ Liệu Lớn & Lắp Ráp Lên DCM (UDS RX - Luồng CanTp)
*Kịch bản này mô tả quá trình nhận và lắp ráp gói dữ liệu chẩn đoán 20 bytes từ mạng CAN bên ngoài đi lên tầng Dcm chẩn đoán.*

```mermaid
sequenceDiagram
    autonumber
    participant Can as CAN Driver (bxCAN)
    participant CanIf as CanIf Module
    participant CanTp as CanTp Module
    participant PduR as PduR Module
    participant Dcm as Dcm Module

    Note over Can, CanTp: Bước 1: Nhận First Frame (FF - 20 bytes)
    Can ->> CanIf: CanIf_RxIndication(0x7E0, FF_Pdu)
    CanIf ->> CanTp: CanTp_RxIndication(0, FF_Pdu)
    Note over CanTp: Giải mã PCI FF: TotalLength = 20<br/>State = CANTP_RX_WAIT_CF<br/>ReceivedLength = 6, SN = 1

    Note over CanTp, Dcm: Bước 2: Báo hiệu & Cấp phát bộ đệm (DCM)
    CanTp ->> PduR: PduR_CanTpStartOfReception(0, NULL, 20, &bufSize)
    PduR ->> Dcm: Dcm_StartOfReception(0, NULL, 20, &bufSize)
    Dcm -->> CanTp: BUFREQ_OK (Buffer khả dụng = 256)

    Note over CanTp, Dcm: Bước 3: Copy 6 bytes dữ liệu đầu tiên từ FF
    CanTp ->> PduR: PduR_CanTpCopyRxData(0, FF_PduData, &bufSize)
    PduR ->> Dcm: Dcm_CopyRxData(0, FF_PduData, &bufSize)
    Dcm -->> CanTp: BUFREQ_OK

    Note over CanTp, Can: Bước 4: Gửi trả lại Flow Control (FC, CTS)
    CanTp ->> CanIf: CanIf_Transmit(4, FC_Pdu)
    CanIf ->> Can: Can_Write(0, FC_Pdu)

    Note over Can, CanTp: Bước 5: Nhận các Consecutive Frames (CF1, CF2)
    Can ->> CanIf: CanIf_RxIndication(0x7E0, CF_Pdu)
    CanIf ->> CanTp: CanTp_RxIndication(0, CF_Pdu)
    Note over CanTp: Trích xuất 7 bytes dữ liệu từ CF
    CanTp ->> PduR: PduR_CanTpCopyRxData(0, CF_PduData, &bufSize)
    PduR ->> Dcm: Dcm_CopyRxData(0, CF_PduData, &bufSize)
    Dcm -->> CanTp: BUFREQ_OK
    Note over CanTp: Cập nhật ReceivedLength += 7

    Note over CanTp, Dcm: Bước 6: Hoàn thành nhận dữ liệu lớn
    Note over CanTp: Khi ReceivedLength >= TotalLength (20 bytes)<br/>1. State = CANTP_IDLE<br/>2. Báo hoàn tất lên tầng trên
    CanTp ->> PduR: PduR_CanTpRxIndication(0, NTFRSLT_OK)
    PduR ->> Dcm: Dcm_TpRxIndication(0, NTFRSLT_OK)
    Note over Dcm: Thực thi xử lý dịch vụ UDS trong Dcm_RxBuffer
```

### Chi tiết hành vi API đồng bộ (Kịch bản 3):
1.  **`CanIf_RxIndication`**: Nhận dạng CAN ID nhận được thuộc dải chẩn đoán (`0x7E0`), chuyển tiếp trực tiếp sang tầng vận chuyển bằng cách gọi:
    *   `CanIf_RxIndication` -> `CanTp_RxIndication(0, PduInfoPtr)`.
2.  **`CanTp_RxIndication` (Xử lý FF)**:
    *   Đọc byte PCI phát hiện First Frame (`PCI = 0x01`).
    *   Giải mã tổng dung lượng `totalRxLength = ((Byte0 & 0x0F) << 8) | Byte1`.
    *   Đưa trạng thái kênh nhận: `RxState.State = CANTP_RX_WAIT_CF`.
    *   Giao tiếp với DCM để xin cấp phát bộ đệm: gọi `PduR_CanTpStartOfReception()`. DCM cấp phát thành công và trả về kích thước đệm khả dụng qua con trỏ `bufferSizePtr`.
    *   Sao chép 6 bytes dữ liệu đầu tiên đi kèm trong FF trực tiếp lên DCM bằng hàm `PduR_CanTpCopyRxData()`.
    *   Tự động kích hoạt phản hồi Flow Control (CTS, BlockSize, STmin) để báo cho bên gửi tiếp tục gửi các mảnh dữ liệu tiếp theo qua CAN bus.
3.  **`CanTp_RxIndication` (Xử lý CF)**:
    *   Mỗi khi nhận được Consecutive Frame, kiểm tra trạng thái đang chờ CF.
    *   Lọc bỏ 1 byte PCI ở đầu CF, trích xuất dữ liệu payload thô (tối đa 7 bytes).
    *   Gọi `PduR_CanTpCopyRxData()` để sao chép trực tiếp dữ liệu thô này vào phân vùng kế tiếp của `Dcm_RxBuffer` (Không copy qua buffer trung gian của CanTp).
    *   Khi `ReceivedLength` đạt đến `TotalLength` (20 bytes), gọi `PduR_CanTpRxIndication(..., NTFRSLT_OK)`.
4.  **`Dcm_TpRxIndication`**: DCM nhận được chỉ thị kết thúc quá trình nhận dữ liệu thành công. Nó ngay lập tức giải mã gói tin (ví dụ: Service ID của dịch vụ UDS) và in log hiển thị dữ liệu ra màn hình Debug.
