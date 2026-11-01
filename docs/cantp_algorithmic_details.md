# Chi Tiết Thuật Toán & Cơ Chế Xử Lý Tập Trung Trong CanTp

Tài liệu này đi sâu phân tích toán học, xử lý bitwise, quản lý máy trạng thái tĩnh và các thuật toán phân mảnh (segmentation) / tái hợp (reassembly) của module **CanTp (CAN Transport Protocol)** dựa trên tiêu chuẩn ISO 15765-2.

---

## 1. Thuật Toán Trích Xuất & Xử Lý Bitwise Byte Điều Khiển PCI

Mọi frame truyền nhận qua CanTp đều được nhận dạng và giải mã dựa trên byte thông tin giao thức **PCI (Protocol Control Information)** nằm ở byte đầu tiên (Byte 0) của payload CAN.

### 1.1. Giải thuật phân loại loại Frame (PCI Type)
Để xác định frame CAN nhận được là loại nào, CanTp áp dụng phép toán bitwise AND kết hợp dịch bit phải:

```c
uint8 pciType = (CanTpRxPduPtr->SduDataPtr[0] & 0xF0) >> 4;
```
*   **Giải thích toán học**: 
    1.  AND với mặt nạ `0xF0` (nhị phân: `1111 0000`) để giữ lại 4 bit cao nhất của byte đầu tiên và xóa bỏ 4 bit thấp.
    2.  Dịch phải 4 bit (`>> 4`) để dịch chuyển giá trị 4 bit cao đó về hàng đơn vị.
    3.  Kết quả `pciType` nằm trong khoảng từ `0x0` đến `0x3` để định danh frame.

### 1.2. Giải thuật trích xuất độ dài gói tin (SduLength)
Tùy thuộc vào loại frame được xác định, thuật toán tính độ dài gói tin sẽ khác nhau:

*   **Đối với Single Frame (SF - `pciType == 0x00`)**:
    Chiều dài dữ liệu nằm gọn trong 4 bit thấp của Byte 0:
    ```c
    uint8 sfLen = CanTpRxPduPtr->SduDataPtr[0] & 0x0F;
    ```
    *Mặt nạ AND với `0x0F` (`0000 1111`) triệt tiêu 4 bit loại frame, chỉ giữ lại 4 bit chứa độ dài.*

*   **Đối với First Frame (FF - `pciType == 0x01`)**:
    Do First Frame hỗ trợ chiều dài thông điệp lên đến 4095 bytes (12-bit), chiều dài được lưu trữ phân mảnh trên 2 bytes:
    ```c
    uint8 fs = CanTpRxPduPtr->SduDataPtr[0] & 0x0F;      // Lấy 4 bit thấp của Byte 0 (chứa 4 bit cao của độ dài)
    uint8 lenLSB = CanTpRxPduPtr->SduDataPtr[1];         // Lấy toàn bộ Byte 1 (chứa 8 bit thấp của độ dài)
    uint16 totalRxLength = (fs << 8) | lenLSB;           // Ghép nối bằng phép toán dịch trái 8 bit và phép OR bitwise
    ```
    *Ví dụ*: Nếu nhận được Byte 0 = `0x10`, Byte 1 = `0x14`:
    1.  `fs = 0x10 & 0x0F = 0`
    2.  `lenLSB = 0x14 = 20`
    3.  `totalRxLength = (0 << 8) | 0x14 = 20` (gói tin dài 20 bytes).

---

## 2. Thuật Toán Phân Mảnh Dữ Liệu Lớn Phía Phát (TX Segmentation)

Khi có yêu cầu truyền dữ liệu từ tầng trên thông qua `CanTp_Transmit`, thuật toán phân đoạn dữ liệu sẽ được thực thi theo các bước tuần tự logic sau:

```
[Bắt đầu truyền]
       │
       ├─► Nếu TotalLength <= 7 ──► [Gửi Single Frame] ──► Kết thúc (IDLE)
       │
       └─► Nếu TotalLength > 7  ──► [Khởi tạo Phân mảnh]
                                          │
                                          ▼
                                   [Gửi First Frame (FF)]
                                   - Lấy 6 bytes dữ liệu đầu
                                   - SentLength = 6, SN = 1
                                   - Trạng thái = WAIT_FC (Chờ nhận FC)
                                          │
                                   [Nhận Flow Control (FC)]
                                          │
                                          ▼
                                   [Gửi CF1 (SN=1) ngay lập tức]
                                   - Lấy min(7, còn lại)
                                   - SentLength += len, SN++
                                   - Trạng thái = WAIT_CF_CONF
                                          │
                                  [TxConfirmation] ◄───────┐
                                          │                 │
                                          ├─► Còn dữ liệu ──┴─► [Gửi CF tiếp theo]
                                          │   (SentLength < TotalLength)
                                          │
                                          └─► Hết dữ liệu ───► Kết thúc (IDLE)
```

### 2.1. Logic tăng tuần hoàn số thứ tự Sequence Number (SN)
Sequence Number (SN) dùng để đánh số thứ tự các Consecutive Frames (CF) nhằm phát hiện mất mát frame. Thuật toán tăng SN được quy định như sau:
*   Bắt đầu từ `SN = 1` ở gói CF đầu tiên ngay sau First Frame.
*   Mỗi lần gửi một CF thành công, `SN` tăng lên 1 đơn vị.
*   Khi `SN` vượt quá 15 (`0x0F`), nó phải quay vòng về `0` (không phải về 1).
*   **Mã nguồn thực thi**:
    ```c
    tx->SN = (tx->SN + 1) & 0x0F;
    ```
    *Phép toán AND với `0x0F` giới hạn giá trị SN trong dải 4-bit (0 đến 15), tự động quay vòng về 0 khi vượt quá 15.*

### 2.2. Thuật toán Pacing (Điều phối tốc độ truyền CF)
Để tránh treo hệ thống và vi phạm luật thời gian giãn cách `STmin` trên bus CAN, CanTp tách biệt hành vi truyền dữ liệu thành một máy trạng thái bất đồng bộ:
*   **Trạng thái `CANTP_TX_WAIT_CF_CONF`**: Chặn không cho phép gửi CF mới khi frame cũ chưa truyền xong trên bus. Chỉ khi Driver phần cứng gọi `CanTp_TxConfirmation()`, CanTp mới chuyển trạng thái kênh sang `CANTP_TX_SEND_CF`.
*   **Trạng thái `CANTP_TX_SEND_CF`**: Cho phép `CanTp_MainFunction()` đóng gói và gửi CF tiếp theo trong chu kỳ quét nền tiếp theo. Sau khi gửi, lập tức đưa trạng thái trở lại `CANTP_TX_WAIT_CF_CONF`.

---

## 3. Thuật Toán Lắp Ráp Dữ Liệu Phía Nhận (RX Reassembly)

Phía nhận chịu trách nhiệm tiếp nhận các frame nhỏ rời rạc (FF và CF) từ CAN Driver, giải mã PCI, trích xuất dữ liệu hữu ích và tích lũy vào bộ đệm của tầng ứng dụng.

### 3.1. Thuật toán Zero-Copy (Đẩy dữ liệu trực tiếp)
Để tối ưu tài nguyên, CanTp RX không duy trì một buffer tích lũy trung gian lớn. Thuật toán nhận sử dụng cơ chế ghi đệm trực tiếp qua `PduR`:

1.  **Nhận FF**: Gọi `PduR_CanTpStartOfReception()` để thông báo chiều dài gói tin. DCM kiểm tra dung lượng buffer tĩnh của mình và trả về quyền ghi dữ liệu.
2.  **Nhận từng mảnh**: Gọi `PduR_CanTpCopyRxData()` kèm theo con trỏ payload thô của CAN frame.
3.  **Sao chép trực tiếp**: Bên trong DCM, dữ liệu được ghi trực tiếp vào offset tăng dần của buffer chẩn đoán:
    ```c
    memcpy(&Dcm_RxBuffer[Dcm_RxLength], info->SduDataPtr, info->SduLength);
    ```

### 3.2. Thuật toán tính toán kích thước copy và chặn tràn (Size Protection)
Khi nhận Consecutive Frame (CF), CanTp nhận được gói tin có thể dài 8 bytes (chứa 1 byte PCI + 7 bytes dữ liệu). Tuy nhiên, ở gói CF cuối cùng, số lượng bytes dữ liệu thực tế cần nhận có thể ít hơn 7 bytes. CanTp thực hiện thuật toán tính toán kích thước thực tế để tránh ghi đè dữ liệu rác:

```c
// 1. Tính toán kích thước payload thô trong CF nhận được (trừ 1 byte PCI)
uint8 len = CanTpRxPduPtr->SduLength - 1;

// 2. Thuật toán bảo vệ chống tràn (Chặn trên):
// Nếu tổng dung lượng đã nhận cộng thêm độ dài frame này vượt quá tổng dung lượng mong đợi
if (RxState.ReceivedLength + len > RxState.TotalLength) {
    // Cắt bớt phần dư thừa, chỉ lấy phần dữ liệu thực tế của thông điệp
    len = RxState.TotalLength - RxState.ReceivedLength;
}

// 3. Tiến hành copy lượng dữ liệu đã được tính toán chuẩn xác lên tầng chẩn đoán
rxDataInfo.SduLength = len;
PduR_CanTpCopyRxData(..., &rxDataInfo, ...);

// 4. Cập nhật tiến trình nhận
RxState.ReceivedLength += len;
```

---

## 4. Phân Tích So Sánh Logic Hiện Tại vs Tiêu Chuẩn ISO 15765-2

Bảng dưới đây chỉ ra chi tiết sự khác biệt thuật toán giữa mã nguồn hiện hành của project (phiên bản rút gọn học tập) và tiêu chuẩn công nghiệp đầy đủ:

| Tính năng | Thuật toán chuẩn ISO-TP | Thuật toán hiện tại trong Code | Đánh giá & Khắc phục |
| :--- | :--- | :--- | :--- |
| **Kiểm lỗi Sequence Number (SN)** | So khớp SN của CF nhận được với SN mong đợi. Nếu sai lệch, hủy phiên nhận và gọi `PduR_CanTpRxIndication(..., NTFRSLT_E_WRONG_SN)`. | Bỏ qua việc so sánh SN. Cứ có CF là trích xuất dữ liệu ghi vào DCM. | **Rủi ro**: Nếu mất gói tin trên bus, dữ liệu lắp ráp bị sai lệch cấu trúc mà không được phát hiện. |
| **Kiểm soát luồng gửi (BlockSize)** | Node phát gửi đủ số lượng gói tin bằng `BlockSize` thì phải dừng truyền CF, chuyển sang trạng thái chờ nhận FC tiếp theo. | Lưu trữ `tx->BlockSize` từ FC nhận được nhưng không đếm số lượng CF đã phát để tạm dừng. | **Rủi ro**: Nếu bên nhận có buffer nhỏ và yêu cầu BS nhỏ để xử lý dần, bên phát sẽ làm tràn ngập buffer bên nhận. |
| **Thời gian giãn cách (STmin)** | Đo khoảng thời gian giữa 2 CF phát đi, đảm bảo lớn hơn hoặc bằng `STmin` (tính bằng ms/us) thông qua Hardware Timer. | Gửi CF tiếp theo ở chu kỳ quét quét nền của Main Function mà không so sánh với timer `STmin`. | **Rủi ro**: Vi phạm thời gian giãn cách tối thiểu làm bên nhận không xử lý kịp ngắt nhận, gây lỗi phần cứng hoặc mất gói tin. |
| **Bộ định thời (Timers)** | Thực thi đếm ngược các timer `N_As`, `N_Bs`, `N_Cs`, `N_Ar`, `N_Br`, `N_Cr` ở Main Function. Khi timer về 0, báo lỗi timeout. | Khai báo các trường timer nhưng không có mã nguồn giảm giá trị timer hay phát hiện timeout. | **Rủi ro**: Nếu bên nhận/phát ngắt kết nối đột ngột, bên còn lại sẽ bị kẹt vĩnh viễn trong trạng thái chờ (Treo kênh truyền). |
