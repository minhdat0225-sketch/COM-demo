# Tài liệu CanTp cực kỳ chi tiết ở mức từng byte

## 1. Mục tiêu tài liệu
Tài liệu này giải thích **luồng truyền và nhận CanTp ở mức từng byte**:
- Bên gửi đóng gói từng byte như thế nào.
- Bên nhận bóc từng byte ra như thế nào.
- Khi gói lớn, bên nhận phản hồi ngược bằng **Flow Control** ra sao.
- Mỗi hàm trong luồng Tx/Rx làm gì.
- Có ví dụ cụ thể cho từng trường hợp.

## 2. Phạm vi và giả định
Tài liệu này được viết theo cách **dễ hiểu và thực dụng**, với các giả định sau:
- Dùng **Classical CAN**.
- Mỗi CAN frame mang tối đa **8 byte dữ liệu**.
- Dùng **normal addressing**.
- Tập trung vào 4 loại frame chính của CanTp:
  - **SF**: Single Frame
  - **FF**: First Frame
  - **CF**: Consecutive Frame
  - **FC**: Flow Control

> Nếu sau đó bạn muốn, tài liệu này có thể mở rộng thêm sang **extended addressing**, **mixed addressing**, hoặc **CAN FD**.

## 3. Bức tranh tổng thể
### 3.1 Luồng truyền từ App xuống bus
1. App hoặc DCM muốn gửi một gói dữ liệu lớn/nhỏ.
2. App gọi xuống PduR.
3. PduR gọi `CanTp_Transmit()`.
4. CanTp quyết định:
   - Gói nhỏ: gửi **Single Frame**.
   - Gói lớn: gửi **First Frame**, rồi chờ **Flow Control** từ bên nhận.
5. Sau đó CanTp chia tiếp thành các **Consecutive Frame**.
6. Mỗi frame được chuyển xuống `CanIf_Transmit()` rồi xuống `Can_Write()`.
7. Hardware phát lên bus.
8. Sau khi phát xong, hardware báo xác nhận ngược lên `CanTp_TxConfirmation()`.

## 3.2 Luồng nhận từ bus lên App
1. Hardware nhận CAN frame.
2. `Can_MainFunction_Read()` hoặc interrupt kéo dữ liệu ra.
3. `CanIf_RxIndication()` đẩy frame cho CanTp.
4. `CanTp_RxIndication()` nhìn byte đầu để biết frame là SF/FF/CF/FC.
5. Nếu là dữ liệu từ bên gửi:
   - Ghép dần vào buffer nhận.
6. Nếu là FF:
   - Bên nhận cấp buffer.
   - Sau đó gửi **Flow Control** ngược lại cho bên gửi.
7. Khi nhận đủ, CanTp báo hoàn tất lên `PduR_CanTpRxIndication()`.

---

## 4. Ý nghĩa 4 loại frame của CanTp

## 4.1 Single Frame (SF)
Dùng khi toàn bộ dữ liệu **đủ nằm trong 1 frame**.

### Cấu trúc byte (Classical CAN, normal addressing)
- **Byte 0**:
  - 4 bit cao: loại frame = `0` (SF)
  - 4 bit thấp: độ dài dữ liệu `DL`
- **Byte 1..Byte n**: dữ liệu thật
- Các byte còn lại: padding nếu cần

### Công thức
- `Byte0 = 0x0 | DL`

Ví dụ `DL = 5`:
- `Byte0 = 0x05`

---

## 4.2 First Frame (FF)
Dùng khi dữ liệu **lớn hơn 7 byte**, không thể nhét vào 1 frame.

### Cấu trúc byte
- **Byte 0**:
  - 4 bit cao: loại frame = `1` (FF)
  - 4 bit thấp: 4 bit cao của tổng chiều dài SDU
- **Byte 1**:
  - 8 bit thấp của tổng chiều dài SDU
- **Byte 2..Byte 7**:
  - 6 byte dữ liệu đầu tiên

### Công thức
Nếu tổng chiều dài là `TotalLen`:
- `Byte0 = 0x10 | ((TotalLen >> 8) & 0x0F)`
- `Byte1 = TotalLen & 0xFF`

Ví dụ `TotalLen = 20 (0x14)`:
- `Byte0 = 0x10`
- `Byte1 = 0x14`

---

## 4.3 Consecutive Frame (CF)
Dùng để gửi **các phần dữ liệu tiếp theo** sau FF.

### Cấu trúc byte
- **Byte 0**:
  - 4 bit cao: loại frame = `2` (CF)
  - 4 bit thấp: `SN` = sequence number
- **Byte 1..Byte 7**:
  - 7 byte dữ liệu tiếp theo

### Sequence Number
SN chạy vòng:
- 1, 2, 3, ..., 15, 0, 1, 2, ...

### Công thức
- `Byte0 = 0x20 | (SN & 0x0F)`

Ví dụ `SN = 1`:
- `Byte0 = 0x21`

---

## 4.4 Flow Control (FC)
Đây là frame **bên nhận gửi ngược lại bên truyền** để điều tiết tốc độ.

### Cấu trúc byte
- **Byte 0**:
  - 4 bit cao: loại frame = `3` (FC)
  - 4 bit thấp: `FS` (Flow Status)
- **Byte 1**: `BS` (Block Size)
- **Byte 2**: `STmin` (minimum separation time)
- **Byte 3..7**: padding

### Flow Status (FS)
- `0` = Continue To Send (CTS)
- `1` = Wait (đợi)
- `2` = Overflow / Abort

### Ví dụ
Bên nhận nói:
- tiếp tục gửi
- mỗi block 4 frame
- cách nhau tối thiểu 20 ms

Thì:
- `Byte0 = 0x30`
- `Byte1 = 0x04`
- `Byte2 = 0x14`

Frame FC:
- `[30] [04] [14] [00] [00] [00] [00] [00]`

---

# 5. Giải thích cực chi tiết từng hàm ở bên truyền

## 5.1 `CanTp_Transmit()` làm gì?
Đây là điểm bắt đầu khi tầng trên muốn gửi dữ liệu.

### Đầu vào
- `TxPduId`: ID logic của gói cần gửi
- `PduInfoPtr`:
  - con trỏ tới buffer dữ liệu
  - độ dài dữ liệu cần gửi

### Công việc chính
1. Kiểm tra kênh Tx có rảnh không.
2. Lưu lại:
   - con trỏ dữ liệu
   - tổng số byte cần gửi
   - số byte đã gửi
   - SN hiện tại
3. Chuyển state sang trạng thái đang truyền.
4. Chờ `CanTp_MainFunction()` xử lý tiếp.

### Ở mức tư duy byte
Ví dụ App đưa vào:
- data = `11 22 33 44 55`
- length = 5

Thì `CanTp_Transmit()` chưa nhất thiết gửi ngay từng byte ra bus, mà chủ yếu:
- giữ buffer nguồn
- ghi nhớ `TotalLength = 5`
- chuẩn bị để hàm main/polling đóng gói frame đúng chuẩn

---

## 5.2 `CanTp_MainFunction()` phía Tx làm gì?
Đây là nơi thực sự quyết định frame nào sẽ được đóng gói và gửi ra.

### Trường hợp A: dữ liệu <= 7 byte
CanTp tạo **Single Frame**.

Ví dụ dữ liệu nguồn:
- `11 22 33 44 55`
- length = 5

### Đóng gói
- Byte0 = `0x05`
- Byte1 = `0x11`
- Byte2 = `0x22`
- Byte3 = `0x33`
- Byte4 = `0x44`
- Byte5 = `0x55`
- Byte6 = padding = `0x00`
- Byte7 = padding = `0x00`

### Frame tạo ra
`05 11 22 33 44 55 00 00`

### Ý nghĩa từng byte
- Byte0 = 0x05:
  - high nibble = 0 => SF
  - low nibble = 5 => có 5 byte dữ liệu thật
- Byte1..5 = payload
- Byte6..7 = padding

Sau đó CanTp gọi `CanIf_Transmit()` với đúng 8 byte này.

---

### Trường hợp B: dữ liệu > 7 byte
CanTp phải tạo **First Frame** trước.

Ví dụ dữ liệu 20 byte:
- `D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF E0 E1 E2 E3`

Tổng độ dài = 20 = `0x14`

### Đóng gói First Frame
- Byte0 = `0x10`
- Byte1 = `0x14`
- Byte2 = `D0`
- Byte3 = `D1`
- Byte4 = `D2`
- Byte5 = `D3`
- Byte6 = `D4`
- Byte7 = `D5`

### Frame FF gửi đi
`10 14 D0 D1 D2 D3 D4 D5`

### Ý nghĩa
- Byte0 = `0x10`
  - high nibble = 1 => FF
  - low nibble = 0 => 4 bit cao của length
- Byte1 = `0x14`
  - 8 bit thấp của tổng length
- Byte2..7 = 6 byte dữ liệu đầu tiên

Sau khi gửi FF:
- CanTp **không gửi CF ngay lập tức**.
- Nó phải **đợi FC từ bên nhận**.

---

## 5.3 `CanIf_Transmit()` làm gì?
Hàm này nhận buffer 8 byte từ CanTp rồi chuyển xuống tầng CAN Interface.

### Nó xử lý gì?
1. Ánh xạ `TxPduId` logic sang CAN ID thật.
2. Chọn mailbox/hardware transmit handle.
3. Gọi `Can_Write()` để nạp frame vào phần cứng.

### Điều quan trọng
`CanIf_Transmit()` **không hiểu sâu nội dung CanTp**. Nó chủ yếu coi 8 byte ấy là payload CAN cần phát.

---

## 5.4 `CanTp_TxConfirmation()` làm gì?
Khi frame đã thực sự phát xong lên bus, hardware xác nhận ngược lên.

### Vai trò
- Biết frame trước đã hoàn tất.
- Cho phép CanTp chuyển bước kế tiếp.

### Ví dụ
Nếu vừa phát xong FF:
- CanTp chuyển sang state `WAIT_FC`
- bắt đầu đếm timeout `N_Bs`
- chờ bên nhận gửi FC

Nếu vừa phát xong một CF:
- cập nhật số byte đã gửi
- tăng SN
- nếu chưa xong, chuẩn bị CF tiếp theo
- nếu đã xong toàn bộ, báo `PduR_CanTpTxConfirmation()`

---

# 6. Giải thích cực chi tiết từng hàm ở bên nhận

## 6.1 `Can_MainFunction_Read()` / interrupt làm gì?
Khi frame xuất hiện trên bus:
1. CAN controller bắt frame.
2. Đưa vào RX mailbox/FIFO.
3. Driver CAN đọc 8 byte dữ liệu và CAN ID.
4. Gọi `CanIf_RxIndication()`.

Ở thời điểm này, dữ liệu vẫn chỉ là:
- CAN ID
- DLC
- 8 byte raw data

Nó chưa ghép lại thành message dài.

---

## 6.2 `CanIf_RxIndication()` làm gì?
1. Xác định frame này thuộc route nào.
2. Từ CAN ID map sang `RxPduId` logic.
3. Gọi `CanTp_RxIndication()`.

Tương tự bên Tx, `CanIf` chỉ làm lớp trung gian.

---

## 6.3 `CanTp_RxIndication()` làm gì?
Đây là hàm quan trọng nhất ở phía nhận.

### Bước đầu tiên
Nó nhìn **Byte0** để xác định frame type.

#### Cách đọc
- `FrameType = (Byte0 >> 4) & 0x0F`
- `LowNibble = Byte0 & 0x0F`

Kết quả:
- `0` => SF
- `1` => FF
- `2` => CF
- `3` => FC

---

# 7. Ví dụ chi tiết từng trường hợp

## 7.1 Trường hợp 1: Single Frame
Giả sử App bên gửi muốn gửi 3 byte:
- `AA BB CC`

### Bên gửi đóng gói
- Byte0 = `0x03`
- Byte1 = `0xAA`
- Byte2 = `0xBB`
- Byte3 = `0xCC`
- Byte4..7 = `00`

Frame phát đi:
`03 AA BB CC 00 00 00 00`

### Bên nhận xử lý từng byte
Nhận 8 byte:
- Byte0 = `03`
- Byte1 = `AA`
- Byte2 = `BB`
- Byte3 = `CC`
- Byte4 = `00`
- Byte5 = `00`
- Byte6 = `00`
- Byte7 = `00`

### Bên nhận bóc tách
- `FrameType = 0` => SF
- `DL = Byte0 & 0x0F = 3`
- Lấy đúng 3 byte dữ liệu:
  - data[0] = Byte1 = `AA`
  - data[1] = Byte2 = `BB`
  - data[2] = Byte3 = `CC`

### Kết quả cuối
Ứng dụng nhận được:
`AA BB CC`

### Điểm cực quan trọng
- Byte4..7 không phải dữ liệu thật.
- Không được copy 8 byte toàn bộ lên App.
- Chỉ copy đúng `DL` byte.

---

## 7.2 Trường hợp 2: Multi-frame 20 byte
Dữ liệu gốc:
`D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF E0 E1 E2 E3`

Tổng 20 byte.

## Giai đoạn 1: Bên gửi gửi FF
### Đóng gói FF
- Byte0 = `10`
- Byte1 = `14`
- Byte2 = `D0`
- Byte3 = `D1`
- Byte4 = `D2`
- Byte5 = `D3`
- Byte6 = `D4`
- Byte7 = `D5`

FF:
`10 14 D0 D1 D2 D3 D4 D5`

### Bên nhận nhận FF
- đọc Byte0 = `10` => frame type = FF
- tính tổng length:
  - high part = `Byte0 & 0x0F = 0`
  - low part = `Byte1 = 0x14`
  - total = `(0 << 8) | 0x14 = 20`

### Bên nhận copy dữ liệu đầu tiên
Từ FF chỉ có 6 byte data:
- buffer[0] = D0
- buffer[1] = D1
- buffer[2] = D2
- buffer[3] = D3
- buffer[4] = D4
- buffer[5] = D5

Lúc này:
- `RxCount = 6`
- `ExpectedLength = 20`
- còn thiếu `14 byte`

---

## Giai đoạn 2: Bên nhận gửi FC ngược lại
Giả sử bên nhận quyết định:
- cho gửi tiếp
- mỗi block 2 frame
- khoảng cách 10 ms

### FC tạo ra
- Byte0 = `30`
- Byte1 = `02`
- Byte2 = `0A`
- Byte3..7 = `00`

FC frame:
`30 02 0A 00 00 00 00 00`

### Ý nghĩa từng byte
- Byte0 = 0x30
  - frame type = FC
  - FS = 0 => Continue To Send
- Byte1 = 0x02
  - BS = 2 => gửi 2 CF thì phải chờ FC tiếp
- Byte2 = 0x0A
  - STmin = 10 ms

---

## Giai đoạn 3: Bên gửi nhận FC và gửi CF1
Bên gửi còn dữ liệu từ `D6` trở đi.

### CF1
SN bắt đầu từ 1.

- Byte0 = `21`
- Byte1 = `D6`
- Byte2 = `D7`
- Byte3 = `D8`
- Byte4 = `D9`
- Byte5 = `DA`
- Byte6 = `DB`
- Byte7 = `DC`

CF1:
`21 D6 D7 D8 D9 DA DB DC`

### Bên nhận xử lý CF1
- frame type = CF
- SN = `1`
- kiểm tra SN mong đợi có đúng là 1 không
- nếu đúng, copy 7 byte vào buffer tiếp theo:
  - buffer[6] = D6
  - buffer[7] = D7
  - buffer[8] = D8
  - buffer[9] = D9
  - buffer[10] = DA
  - buffer[11] = DB
  - buffer[12] = DC

Lúc này:
- `RxCount = 13`

---

## Giai đoạn 4: Bên gửi gửi CF2
Còn dữ liệu:
- `DD DE DF E0 E1 E2 E3`

SN = 2

### CF2
- Byte0 = `22`
- Byte1 = `DD`
- Byte2 = `DE`
- Byte3 = `DF`
- Byte4 = `E0`
- Byte5 = `E1`
- Byte6 = `E2`
- Byte7 = `E3`

CF2:
`22 DD DE DF E0 E1 E2 E3`

### Bên nhận xử lý CF2
- frame type = CF
- SN = 2
- kiểm tra SN expected = 2
- copy vào buffer:
  - buffer[13] = DD
  - buffer[14] = DE
  - buffer[15] = DF
  - buffer[16] = E0
  - buffer[17] = E1
  - buffer[18] = E2
  - buffer[19] = E3

Lúc này:
- `RxCount = 20`
- bằng `ExpectedLength`
- hoàn tất message

### Buffer cuối cùng ở bên nhận
`D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF E0 E1 E2 E3`

---

# 8. Khi block size không bằng 0 thì chuyện gì xảy ra?
Ví dụ message dài hơn nữa.

Nếu bên nhận gửi:
- `BS = 3`

Nghĩa là:
1. Sau FF, bên gửi chỉ được gửi 3 CF.
2. Gửi xong CF thứ 3 phải dừng.
3. Chờ bên nhận gửi FC mới.
4. Nếu FC mới cho phép tiếp thì mới gửi tiếp.

Điều này giúp bên nhận:
- tránh tràn buffer
- có thời gian xử lý
- điều tiết tốc độ thực tế

---

# 9. Khi bên nhận gửi Wait thì sao?
Ví dụ FC:
`31 00 00 00 00 00 00 00`

### Phân tích
- Byte0 = `31`
  - frame type = FC
  - FS = 1 => Wait
- Byte1, Byte2 lúc này ít ý nghĩa hơn so với CTS

### Hiểu đúng
Bên nhận đang nói:
- “Tôi chưa sẵn sàng, hãy chờ.”

### Bên gửi làm gì?
- giữ nguyên trạng thái truyền
- chưa gửi CF tiếp
- chờ FC mới hoặc timeout

---

# 10. Khi bên nhận báo overflow thì sao?
Ví dụ FC:
`32 00 00 00 00 00 00 00`

### Ý nghĩa
- FS = 2 => overflow / abort

Bên nhận đang nói:
- “Tôi không đủ khả năng nhận tiếp.”

### Bên gửi phải làm gì?
- hủy phiên truyền
- giải phóng state
- báo lỗi lên tầng trên

---

# 11. Cực kỳ chi tiết: SN tăng như thế nào
Sau FF, CF đầu tiên luôn thường được mong đợi là SN = 1.

Chuỗi SN:
- CF1 -> SN=1 -> Byte0=0x21
- CF2 -> SN=2 -> Byte0=0x22
- ...
- CF15 -> SN=15 -> Byte0=0x2F
- CF16 -> SN=0 -> Byte0=0x20
- CF17 -> SN=1 -> Byte0=0x21

### Bên nhận kiểm tra ra sao?
Nó giữ biến `ExpectedSN`.

Ví dụ:
- sau FF: `ExpectedSN = 1`
- nhận CF có Byte0 = `0x21` => hợp lệ
- cập nhật `ExpectedSN = 2`
- nhận CF có Byte0 = `0x22` => hợp lệ
- cập nhật `ExpectedSN = 3`

Nếu đang đợi SN=3 mà lại nhận `0x25`:
- sai thứ tự
- báo lỗi
- hủy reassembly

---

# 12. Cực kỳ chi tiết: copy từng byte vào buffer như thế nào

## 12.1 Với FF
FF chỉ mang 6 byte data.

Pseudo-code:

```c
rxBuffer[0] = frame[2];
rxBuffer[1] = frame[3];
rxBuffer[2] = frame[4];
rxBuffer[3] = frame[5];
rxBuffer[4] = frame[6];
rxBuffer[5] = frame[7];
rxCount = 6;
```

## 12.2 Với mỗi CF
Mỗi CF mang tối đa 7 byte data.

```c
for (i = 1; i < 8; i++) {
    if (rxCount < expectedLength) {
        rxBuffer[rxCount] = frame[i];
        rxCount++;
    }
}
```

### Điểm rất quan trọng
CF cuối có thể chứa dư padding ở cuối frame.
Do đó phải kiểm tra:
- chỉ copy khi `rxCount < expectedLength`
- không copy quá số byte thực cần nhận

---

# 13. Ví dụ có padding ở frame cuối
Giả sử tổng length = 18 byte.

- FF mang 6 byte đầu.
- Còn lại 12 byte.
- CF1 mang 7 byte.
- CF2 chỉ cần 5 byte cuối.

### CF2 có thể trông như sau
`22 B1 B2 B3 B4 B5 00 00`

Trong đó:
- `B1..B5` là data thật
- 2 byte cuối chỉ là padding

### Bên nhận phải làm gì?
Nếu hiện tại đã nhận 13 byte và cần đủ 18 byte:
- chỉ lấy thêm 5 byte
- bỏ qua 2 byte cuối

Pseudo-code:
```c
remaining = expectedLength - rxCount;
copyLen = (remaining >= 7) ? 7 : remaining;
for (i = 0; i < copyLen; i++) {
    rxBuffer[rxCount++] = frame[i + 1];
}
```

---

# 14. Luồng “truyền ngược” từ bên nhận sang bên gửi thực chất là gì?
Đây chính là **Flow Control**.

Nó không phải “gửi ngược lại payload gốc”, mà là **gửi ngược thông điệp điều khiển**.

## 14.1 Khi nào gửi FC?
- Sau khi nhận FF và đã cấp được buffer.
- Sau mỗi block nếu `BS != 0`.
- Khi cần bảo bên gửi chờ (`Wait`).
- Khi không nhận nổi nữa (`Overflow`).

## 14.2 FC đi qua những hàm nào?
1. Bên nhận nhận FF trong `CanTp_RxIndication()`.
2. Bên nhận xin/có buffer.
3. CanTp tạo frame FC 8 byte.
4. Gọi `CanIf_Transmit()`.
5. Hardware phát FC lên bus.
6. Bên gửi nhận FC thông qua `CanIf_RxIndication()` rồi `CanTp_RxIndication()` hoặc logic Rx tương ứng cho FC.
7. Bên gửi đọc:
   - FS
   - BS
   - STmin
8. Bên gửi điều chỉnh nhịp gửi CF.

---

# 15. State machine dễ hiểu

## 15.1 Bên gửi
- `IDLE`
- `SEND_SF`
- `SEND_FF`
- `WAIT_FC`
- `SEND_CF`
- `WAIT_TX_CONFIRM`
- `FINISHED`
- `ERROR`

## 15.2 Bên nhận
- `IDLE`
- `RECEIVE_SF`
- `RECEIVE_FF`
- `SEND_FC`
- `RECEIVE_CF`
- `FINISHED`
- `ERROR`

---

# 16. Pseudo-code rất dễ hiểu cho bên gửi

```c
CanTp_Transmit(data, len)
{
    tx.buf = data;
    tx.totalLen = len;
    tx.sent = 0;
    tx.sn = 1;
    tx.state = START;
}

CanTp_MainFunction_Tx()
{
    if (tx.state == START) {
        if (tx.totalLen <= 7) {
            frame[0] = tx.totalLen;
            for (i = 0; i < tx.totalLen; i++) {
                frame[i + 1] = tx.buf[i];
            }
            pad_remaining_bytes(frame);
            CanIf_Transmit(frame);
            tx.state = WAIT_TX_CONFIRM_FINAL;
        } else {
            frame[0] = 0x10 | ((tx.totalLen >> 8) & 0x0F);
            frame[1] = tx.totalLen & 0xFF;
            for (i = 0; i < 6; i++) {
                frame[i + 2] = tx.buf[i];
            }
            CanIf_Transmit(frame);
            tx.sent = 6;
            tx.state = WAIT_FC;
        }
    }
    else if (tx.state == SEND_CF_ALLOWED) {
        frame[0] = 0x20 | (tx.sn & 0x0F);
        copy_next_7_bytes_or_less();
        CanIf_Transmit(frame);
        tx.sn = (tx.sn + 1) & 0x0F;
    }
}
```

---

# 17. Pseudo-code rất dễ hiểu cho bên nhận

```c
CanTp_RxIndication(frame)
{
    type = (frame[0] >> 4) & 0x0F;

    if (type == 0) {
        len = frame[0] & 0x0F;
        for (i = 0; i < len; i++) {
            rx.buf[i] = frame[i + 1];
        }
        notify_upper_layer_ok();
    }
    else if (type == 1) {
        rx.totalLen = ((frame[0] & 0x0F) << 8) | frame[1];
        allocate_rx_buffer(rx.totalLen);
        for (i = 0; i < 6; i++) {
            rx.buf[i] = frame[i + 2];
        }
        rx.count = 6;
        rx.expectedSn = 1;
        send_flow_control_cts();
    }
    else if (type == 2) {
        sn = frame[0] & 0x0F;
        if (sn != rx.expectedSn) {
            abort_reception();
            return;
        }
        for (i = 1; i < 8; i++) {
            if (rx.count < rx.totalLen) {
                rx.buf[rx.count++] = frame[i];
            }
        }
        rx.expectedSn = (rx.expectedSn + 1) & 0x0F;
        if (rx.count == rx.totalLen) {
            notify_upper_layer_ok();
        }
    }
}
```

---

# 18. Các lỗi rất hay gặp trong code thật

## 18.1 Copy quá số byte cần nhận
Lỗi phổ biến nhất ở CF cuối.

Sai:
```c
for (i = 1; i < 8; i++) {
    rx.buf[rx.count++] = frame[i];
}
```

Đúng:
```c
for (i = 1; i < 8; i++) {
    if (rx.count < rx.totalLen) {
        rx.buf[rx.count++] = frame[i];
    }
}
```

## 18.2 Không kiểm tra SN
Nếu bỏ SN, dữ liệu có thể ghép sai thứ tự mà không phát hiện.

## 18.3 Không xử lý timeout
- Bên gửi chờ FC mãi.
- Bên nhận chờ CF mãi.

Phải có timeout:
- `N_As`
- `N_Bs`
- `N_Cs`
- `N_Cr`

## 18.4 Dùng nhầm số byte payload ở FF
FF mang **6 byte data**, không phải 7.

## 18.5 Dùng nhầm số byte payload ở CF
CF mang **7 byte data**, Byte0 là header, không phải data.

---

# 19. Bảng nhớ nhanh cực dễ học

## 19.1 Mỗi frame chở bao nhiêu byte data?
- SF: tối đa 7 byte data
- FF: 6 byte data đầu
- CF: 7 byte data mỗi frame
- FC: không chở payload ứng dụng, chỉ chở điều khiển

## 19.2 Byte đầu tiên nói gì?
- `0x0x` => SF
- `0x1x` => FF
- `0x2x` => CF
- `0x3x` => FC

## 19.3 Bên nhận gửi ngược cái gì?
- Không gửi ngược payload gốc
- Gửi ngược **FC = lệnh điều tiết truyền**

---

# 20. Kết luận dễ hiểu nhất
Nếu nói cực ngắn gọn:

- **Bên gửi** lấy mảng dữ liệu gốc, cắt thành từng miếng theo luật CanTp.
- **Byte0** của mỗi frame luôn là byte rất quan trọng vì nó cho biết frame là loại gì.
- **SF** dùng cho gói ngắn.
- **FF + CF** dùng cho gói dài.
- **Bên nhận** phải đọc Byte0 trước, rồi mới biết bóc phần còn lại ra sao.
- Với gói dài, **bên nhận sẽ gửi FC ngược lại** để bảo bên gửi:
  - gửi tiếp,
  - chờ,
  - hay dừng luôn.
- Khi nhận CF, bên nhận phải:
  - kiểm tra SN,
  - copy đúng số byte,
  - không copy padding dư,
  - ghép lại đủ chiều dài ban đầu.

---

# 21. Nguồn gốc nội dung tài liệu này
Tài liệu này được mở rộng trực tiếp từ hai tài liệu Tx/Rx mà bạn đã cung cấp, trong đó phần Tx mô tả luồng `CanTp_Transmit() -> CanTp_MainFunction() -> CanIf_Transmit() -> CanTp_TxConfirmation()` và phần Rx mô tả luồng `Can_MainFunction_Read() -> CanIf_RxIndication() -> CanTp_RxIndication() -> PduR_CanTpRxIndication()` cùng cơ chế Flow Control, SN và timeout.
