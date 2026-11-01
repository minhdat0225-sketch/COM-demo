# Đặc Tả eVCU Và Đề Bài Cho Hai ECU Học Viên

## 1. Bối Cảnh

Mục tiêu của bài này là xây dựng một chương trình `eVCU` chạy trên project `Os_Test`.
Chương trình này sẽ được build thành một file firmware `.elf`. File `.elf` này đóng vai trò
một ECU đối tác đã có sẵn để học viên kiểm thử bài làm của mình.

Học viên không viết eVCU. Học viên sẽ viết 2 ECU khác:

- Một ECU giao tiếp với eVCU bằng UDS qua CAN TP/DCM.
- Một ECU giao tiếp với eVCU bằng AUTOSAR COM qua CAN.

Nói cách khác, eVCU là node chuẩn do giảng viên cung cấp. Hai ECU của học viên phải giao tiếp
đúng protocol với eVCU thì bài làm mới được xem là đạt.

## 2. Vai Trò Của eVCU

eVCU là một Vehicle Control Unit giả lập. Nó không điều khiển xe thật, mà mô phỏng một ECU trung tâm
trong xe để phục vụ đào tạo AUTOSAR Classic.

eVCU có 2 interface độc lập:

| Interface | Dùng để kiểm thử | Stack liên quan |
| --- | --- | --- |
| Diagnostic interface | UDS request/response, DTC, DID, CAN TP multi-frame | CAN, CanIf, CanTp, PduR, DCM |
| COM interface | Gửi/nhận signal định kỳ giữa ECU | CAN, CanIf, PduR, COM |

Trong implementation hiện tại ở `Os_Test`, eVCU được build thành:

```text
build/os_test/os_test.elf
```

Sau này có thể đổi tên target thành:

```text
build/evcu/evcu.elf
```

Tùy tổ chức Makefile sau này.

## 3. Mô Hình Bài Tập

Hệ thống có 3 ECU logic:

| ECU | Người viết | Vai trò |
| --- | --- | --- |
| eVCU | Giảng viên cung cấp | ECU đối tác, build sẵn thành `.elf` |
| Diagnostic ECU | Học viên | Gửi UDS request tới eVCU và kiểm tra response |
| COM ECU | Học viên | Giao tiếp signal với eVCU qua AUTOSAR COM |

Sơ đồ tổng quát:

```text
                 UDS over CAN TP
Diagnostic ECU  ---------------->  eVCU
Diagnostic ECU  <----------------  eVCU

                    COM over CAN
COM ECU         ---------------->  eVCU
COM ECU         <----------------  eVCU
```

## 4. eVCU Sẽ Là Gì?

eVCU là một ECU giả lập có dữ liệu xe nội bộ.

Nó lưu các trạng thái sau:

| Trạng thái | Ý nghĩa |
| --- | --- |
| `ignition_state` | OFF/ON |
| `vehicle_speed` | Tốc độ xe giả lập |
| `engine_rpm` | RPM nhận từ COM ECU hoặc giá trị mặc định |
| `engine_temp` | Nhiệt độ động cơ nhận từ COM ECU hoặc giá trị mặc định |
| `battery_voltage` | Điện áp ắc quy giả lập |
| `torque_request` | Yêu cầu torque eVCU gửi cho COM ECU |
| `alive_counter` | Counter tăng theo chu kỳ để kiểm tra truyền thông |
| `dtc_table` | Danh sách mã lỗi giả lập |

Các trạng thái này được dùng cho cả UDS và COM:

- Diagnostic ECU có thể đọc `engine_rpm`, `engine_temp`, VIN, software version, DTC qua UDS.
- COM ECU có thể gửi `EngineStatus` để cập nhật `engine_rpm`, `engine_temp`.
- eVCU gửi `VehicleCommand` định kỳ cho COM ECU.
- Nếu COM ECU không gửi `EngineStatus` trong một khoảng timeout, eVCU set DTC mất tín hiệu.

## 5. Yêu Cầu Cho eVCU `.elf`

eVCU `.elf` phải có các chức năng:

1. Khởi tạo BSW:
   - `Can_Init()`
   - `CanIf_Init()`
   - `CanTp_Init()`
   - `PduR_Init()`
   - `Com_Init()`
   - `Dcm_Init()`

2. Chạy các main function định kỳ:
   - `Can_MainFunction_Read()`
   - `Can_MainFunction_Write()`
   - `CanTp_MainFunction()`
   - COM/eVCU application cyclic task

3. Nhận UDS request từ Diagnostic ECU.

4. Trả UDS response đúng service.

5. Có ít nhất một UDS response dài hơn 20 byte để bắt buộc chạy CAN TP multi-frame.

6. Nhận COM I-PDU từ COM ECU.

7. Gửi COM I-PDU định kỳ cho COM ECU.

8. Có log UART để giảng viên/học viên debug:

```text
[EVCU] Boot
[EVCU][UDS] RX SID=0x22 LEN=3
[EVCU][UDS] TX POS LEN=20+
[EVCU][COM] RX EngineStatus rpm=3000 temp=90
[EVCU][COM] TX VehicleCommand alive=1
```

## 6. Kiến Trúc Task Trên Os_Test

Đề xuất cấu trúc task:

```text
Task_EvcuInit
  -> khởi tạo UART log
  -> khởi tạo CAN/CanIf/CanTp/PduR/COM/DCM
  -> khởi tạo model nội bộ eVCU

Task_CanMain, chu kỳ 1 ms
  -> Can_MainFunction_Read()
  -> Can_MainFunction_Write()

Task_CanTpMain, chu kỳ 5 ms
  -> CanTp_MainFunction()

Task_EvcuComMain, chu kỳ 10 ms
  -> đọc COM RX signal
  -> cập nhật DTC timeout
  -> gửi COM TX signal

Task_EvcuAppMain, chu kỳ 10 ms
  -> cập nhật vehicle model
  -> cập nhật alive counter
```

Luồng UDS:

```text
Diagnostic ECU
  -> CAN frame
  -> CanIf_RxIndication()
  -> CanTp_RxIndication()
  -> PduR_CanTpCopyRxData()
  -> Dcm_TpRxIndication()
  -> Evcu_DcmProcessRequest()
  -> CanTp_Transmit()
  -> CanIf_Transmit()
  -> CAN frame
```

Luồng COM:

```text
COM ECU
  -> CAN frame EngineStatus
  -> CanIf_RxIndication()
  -> PduR_CanIfRxIndication()
  -> Com_RxIndication()
  -> Evcu_ComReadSignals()

eVCU
  -> Evcu_ComWriteSignals()
  -> Com_SendSignal()
  -> Com_TriggerIPDUSend()
  -> PduR_ComTransmit()
  -> CanIf_Transmit()
  -> CAN frame VehicleCommand
```

## 7. CAN ID Cho Bài Tập

### 7.1 Diagnostic CAN ID

| Hướng | CAN ID | Mô tả |
| --- | ---: | --- |
| Diagnostic ECU -> eVCU | `0x7E0` | UDS physical request |
| eVCU -> Diagnostic ECU | `0x7E8` | UDS physical response |
| Diagnostic ECU -> eVCU | `0x7DF` | UDS functional request, optional |

Quy ước cho bài học:

- Học viên nên dùng `0x7E0` để gửi request chẩn đoán.
- eVCU luôn trả response trên `0x7E8`.
- Với message dài hơn 7 byte, hai bên phải dùng CAN TP.

### 7.2 COM CAN ID

| I-PDU | CAN ID | Hướng theo eVCU | DLC | Mô tả |
| --- | ---: | --- | ---: | --- |
| `EngineStatus` | `0x181` | RX | 8 | COM ECU gửi trạng thái động cơ cho eVCU |
| `VehicleCommand` | `0x180` | TX | 8 | eVCU gửi command/status cho COM ECU |

Có thể mở rộng thêm:

| I-PDU | CAN ID | Hướng theo eVCU | DLC | Mô tả |
| --- | ---: | --- | ---: | --- |
| `BrakeCommand` | `0x280` | TX | 8 | eVCU gửi brake/regen command |
| `BodyCommand` | `0x380` | TX | 8 | eVCU gửi body/light command |

Trong bài đầu tiên, chỉ cần bắt buộc `EngineStatus` và `VehicleCommand`.

## 8. Protocol UDS Giữa Diagnostic ECU Và eVCU

UDS payload không bao gồm ISO-TP PCI byte.

### 8.1 Service `0x10` - DiagnosticSessionControl

Diagnostic ECU gửi:

```text
10 01
```

eVCU trả:

```text
50 01 00 32 01 F4
```

Diagnostic ECU gửi:

```text
10 03
```

eVCU trả:

```text
50 03 00 32 01 F4
```

Nếu sub-function không hỗ trợ:

```text
7F 10 12
```

### 8.2 Service `0x22` - ReadDataByIdentifier

Diagnostic ECU gửi:

```text
22 DID_H DID_L
```

eVCU trả:

```text
62 DID_H DID_L data...
```

Các DID bắt buộc:

| DID | Tên | Response data | Mục đích |
| ---: | --- | --- | --- |
| `0xF190` | VIN | 17 byte ASCII | Kiểm tra UDS DID cơ bản |
| `0xF189` | SoftwareVersion | 8 byte ASCII | Kiểm tra đọc thông tin ECU |
| `0x010C` | Engine RPM | 2 byte big-endian | Kiểm tra dữ liệu lấy từ COM |
| `0x0105` | Coolant Temperature | 1 byte | Kiểm tra dữ liệu lấy từ COM |
| `0xF001` | eVCU Snapshot | 24 byte binary | Bắt buộc test CAN TP response > 20 byte |

Ví dụ request VIN:

```text
22 F1 90
```

Ví dụ response VIN:

```text
62 F1 90 45 56 43 55 53 49 4D 30 30 30 30 30 30 30 30 31
```

Chuỗi ASCII là:

```text
EVCUSIM0000000001
```

Ví dụ request snapshot:

```text
22 F0 01
```

Response snapshot phải dài hơn 20 byte:

```text
62 F0 01
  ignition_state
  vehicle_speed
  engine_rpm_hi engine_rpm_lo
  engine_temp
  battery_voltage_raw
  torque_request
  dtc_count
  alive_counter
  communication_state
  reserved bytes...
```

Nếu DID không hỗ trợ:

```text
7F 22 31
```

### 8.3 Service `0x19` - ReadDTCInformation

Diagnostic ECU gửi:

```text
19 02 FF
```

Ý nghĩa:

- `0x19`: ReadDTCInformation
- `0x02`: ReportDTCByStatusMask
- `0xFF`: đọc tất cả DTC theo status mask

eVCU trả:

```text
59 02 FF DTC1_H DTC1_M DTC1_L status DTC2_H ...
```

DTC giả lập trong eVCU là latched DTC: khi active thì giữ nguyên cho tới khi Diagnostic ECU
gửi `ClearDiagnosticInformation`. Mục đích là đảm bảo `ReadDTCInformation` có response dài
hơn 20 byte để học viên phải xử lý CAN TP multi-frame.

DTC giả lập:

| DTC | Ý nghĩa | Status |
| ---: | --- | ---: |
| `0x123456` | Mất tín hiệu EngineStatus từ COM ECU | `0x2F` |
| `0x234567` | Nhiệt độ động cơ cao | `0x2F` |
| `0x345678` | Điện áp ắc quy thấp | `0x2F` |
| `0x456789` | Lỗi giả lập cho bài CAN TP long response | `0x09` |
| `0x56789A` | Lỗi giả lập mở rộng | `0x09` |

Response có 5 DTC sẽ dài hơn 20 byte:

```text
59 02 FF
12 34 56 2F
23 45 67 2F
34 56 78 2F
45 67 89 09
56 78 9A 09
```

### 8.4 Service `0x14` - ClearDiagnosticInformation

Diagnostic ECU gửi trong extended session:

```text
14 FF FF FF
```

eVCU trả:

```text
54
```

Nếu gửi `0x14` khi chưa vào extended session:

```text
7F 14 22
```

Nếu groupOfDTC không hỗ trợ:

```text
7F 14 31
```

### 8.5 Service Không Hỗ Trợ

Nếu Diagnostic ECU gửi service chưa hỗ trợ, ví dụ:

```text
27 01
```

eVCU trả:

```text
7F 27 11
```

## 9. Protocol CAN TP

Học viên viết Diagnostic ECU phải hỗ trợ ISO-TP tối thiểu.

### 9.1 Single Frame

Dùng khi UDS payload <= 7 byte:

```text
Byte0 = 0x00 | length
Byte1..Byte7 = UDS payload
```

Ví dụ gửi `22 F1 90`:

```text
03 22 F1 90 00 00 00 00
```

### 9.2 First Frame

Dùng khi UDS payload > 7 byte:

```text
Byte0 = 0x10 | ((length >> 8) & 0x0F)
Byte1 = length & 0xFF
Byte2..Byte7 = 6 byte đầu
```

### 9.3 Flow Control

Bên nhận gửi Flow Control sau khi nhận First Frame:

```text
30 00 00
```

hoặc:

```text
30 08 14
```

Ý nghĩa:

- `0x30`: Continue To Send.
- `0x00` hoặc `0x08`: Block Size.
- `0x00` hoặc `0x14`: STmin.

### 9.4 Consecutive Frame

Bên truyền gửi các frame tiếp theo:

```text
Byte0 = 0x20 | sequence_number
Byte1..Byte7 = data
```

Sequence number chạy:

```text
1, 2, 3, ..., 15, 0, 1, ...
```

## 10. Protocol COM Giữa COM ECU Và eVCU

COM ECU của học viên phải giao tiếp được với eVCU bằng I-PDU.

### 10.1 `EngineStatus`, COM ECU -> eVCU

CAN ID: `0x181`, DLC 8.

| Byte/bit | Signal | Type | Scale | Mô tả |
| --- | --- | --- | --- | --- |
| byte 0..1 | `Engine_RPM` | uint16 BE | 1 rpm/bit | Tốc độ động cơ |
| byte 2 | `Engine_Temp` | uint8 | 1 degC/bit | Nhiệt độ nước |
| byte 3 | `Engine_TorqueActual` | uint8 | 1 Nm/bit | Torque hiện tại |
| byte 4 | `Engine_State` | uint8 | enum | 0 off, 1 crank, 2 run, 3 fault |
| byte 5 low nibble | `Alive` | uint4 | raw | Alive counter |
| byte 5 high nibble | `CRC` | uint4 | raw | Checksum đơn giản |
| byte 6..7 | `Reserved` | uint8 | raw | Để mở rộng |

Ví dụ COM ECU gửi:

```text
0B B8 5A 64 02 1A 00 00
```

Ý nghĩa:

- RPM = `0x0BB8` = 3000 rpm.
- Temp = `0x5A` = 90 độ C.
- TorqueActual = 100 Nm.
- Engine_State = 2, đang run.
- Alive = `0xA`.
- CRC = `0x1`.

eVCU sau khi nhận frame này phải cập nhật:

- DID `0x010C` trả RPM = 3000.
- DID `0x0105` trả temp = 90.

### 10.2 `VehicleCommand`, eVCU -> COM ECU

CAN ID: `0x180`, DLC 8.

| Byte/bit | Signal | Type | Scale | Mô tả |
| --- | --- | --- | --- | --- |
| byte 0 | `ThrottleReq` | uint8 | percent | Yêu cầu ga |
| byte 1 bit 0 | `EngineStartReq` | boolean | raw | Yêu cầu start |
| byte 2 | `TorqueLimit` | uint8 | Nm | Giới hạn torque |
| byte 3 low nibble | `Alive` | uint4 | raw | Alive counter |
| byte 3 high nibble | `CRC` | uint4 | raw | Checksum đơn giản |
| byte 4..7 | `Reserved` | uint8 | raw | Để mở rộng |

eVCU gửi frame này định kỳ, ví dụ mỗi 10 ms hoặc 20 ms.

COM ECU của học viên phải:

- Nhận được CAN ID `0x180`.
- Unpack được các signal.
- Kiểm tra alive counter thay đổi theo thời gian.

## 11. Đề Bài Cho Học Viên

### 11.1 Bài 1 - Diagnostic ECU

Học viên viết một ECU chẩn đoán giao tiếp với eVCU.

Yêu cầu tối thiểu:

1. Gửi UDS `10 03` để vào extended session.
2. Gửi UDS `22 F1 90` để đọc VIN.
3. Gửi UDS `22 F0 01` để đọc snapshot dài hơn 20 byte.
4. Gửi UDS `19 02 FF` để đọc danh sách DTC.
5. Gửi UDS `14 FF FF FF` để xóa DTC.
6. Xử lý được CAN TP multi-frame response từ eVCU.
7. In log request/response rõ ràng.

Tiêu chí đạt:

- Parse đúng positive response `0x50`, `0x62`, `0x59`, `0x54`.
- Parse đúng negative response `0x7F`.
- Nhận đầy đủ response dài hơn 20 byte.
- Không mất thứ tự Consecutive Frame.

### 11.2 Bài 2 - COM ECU

Học viên viết một ECU giao tiếp COM với eVCU.

Yêu cầu tối thiểu:

1. Gửi `EngineStatus` trên CAN ID `0x181` định kỳ.
2. Pack đúng RPM, temperature, torque, state, alive, CRC.
3. Nhận `VehicleCommand` từ eVCU trên CAN ID `0x180`.
4. Unpack đúng throttle request, start request, torque limit, alive, CRC.
5. Tăng alive counter theo chu kỳ.

Tiêu chí đạt:

- eVCU đọc được RPM/temp từ COM ECU.
- Diagnostic ECU đọc DID `0x010C` và `0x0105` thấy đúng dữ liệu COM ECU đã gửi.
- COM ECU nhận được `VehicleCommand` định kỳ từ eVCU.

### 11.3 Bài Tích Hợp - Hai ECU Cùng Giao Tiếp Với eVCU

Chạy đồng thời:

- eVCU `.elf` của giảng viên.
- Diagnostic ECU của học viên.
- COM ECU của học viên.

Kịch bản:

1. COM ECU gửi `EngineStatus` với RPM = 3000, temp = 90.
2. Diagnostic ECU gửi `22 01 0C`.
3. eVCU trả RPM = 3000.
4. Diagnostic ECU gửi `22 01 05`.
5. eVCU trả temp = 90.
6. Dừng COM ECU hoặc ngừng gửi `EngineStatus`.
7. eVCU set DTC `0x123456`.
8. Diagnostic ECU gửi `19 02 FF`.
9. eVCU trả DTC mất tín hiệu EngineStatus.

## 12. Definition Of Done Cho eVCU

eVCU hoàn thành khi:

- Build được thành `.elf`.
- Boot được trên Renode hoặc board STM32F103.
- Có log boot eVCU.
- Trả lời được UDS `10 03`.
- Trả lời được UDS `22 F1 90`.
- Trả lời được UDS `22 F0 01` với response dài hơn 20 byte.
- Trả lời được UDS `19 02 FF` với danh sách DTC.
- Xử lý được UDS `14 FF FF FF`.
- Nhận được `EngineStatus` từ COM ECU.
- Gửi được `VehicleCommand` định kỳ cho COM ECU.
- Dữ liệu nhận từ COM ECU có thể đọc lại qua UDS DID.

## 13. Hướng Dẫn Sử Dụng Công Cụ Kiểm Thử (Interactive Testing Tool)

Hệ thống cung cấp một công cụ kiểm thử đa năng viết bằng Python tại `Os_Test/Tool/evcu_tester.py` để giúp giảng viên và học viên dễ dàng đánh giá firmware.

**Cách khởi chạy:**
Mở Terminal tại thư mục `Os_Test` và chạy lệnh:
```bash
./Tool/evcu_tester.py
```

**Các chế độ kiểm thử (Modes):**
Công cụ cung cấp giao diện tương tác với 4 chế độ:
- **Mode 1: Kiểm thử Nội bộ eVCU (Dummy UDS/COM)**: Chạy eVCU độc lập trên Renode, sử dụng GDB để bơm tín hiệu giả lập. Công cụ tự động test tất cả các dịch vụ UDS (`10 03`, `22 F1 90`, `22 F0 01`, `19 02 FF`, `14 FF FF FF`, `27 01`) và kiểm tra logic nhận/gửi COM.
- **Mode 2 & 3**: Chạy mạng ảo gồm 2 ECU để debug thủ công (manual) qua GDB và phân tích UART log.
- **Mode 4: Kiểm thử Tích hợp Toàn hệ thống**: Giả lập mạng CAN thật gồm cả 3 ECU. Tool tự động kích hoạt gửi các khung CAN TP thật giữa các ECU và xác minh toàn vẹn dữ liệu từ đầu đến cuối, bao gồm cả kịch bản **timeout mất tín hiệu COM** và kiểm tra eVCU có thiết lập đúng mã lỗi `0x123456` hay không.

**Cấu hình tự động:**
- Tool tự động sinh ra kịch bản phần cứng `dynamic.resc` tương ứng với Mode đã chọn (1, 2 hay 3 Node).
- Không cần cấu hình file tĩnh, Tool sẽ gợi ý các đường dẫn file `.elf` mặc định của khóa học. Bạn chỉ cần nhấn `Enter` để duyệt nhanh.

**Ví dụ Nhật ký giao tiếp (TX/RX Log):**
Khi chạy, Tool sẽ in ra payload dạng Hexa cực kỳ tường minh phục vụ cho việc chấm điểm và debug:
```text
[Bước 1] Khởi tạo Session Control qua CAN TP (10 03)
    [UDS TX] 10 03
    [UDS RX] 50 03 00 32 01 F4

[Bước 7] Tích hợp mạng COM - Gửi bản tin EngineStatus từ COM ECU
    [COM] Kích hoạt vòng lặp truyền: EngineStatus (RPM=3000, Temp=90) trên CAN Bus
    [COM] Kiểm tra COM ECU có đang nhận được gói VehicleCommand từ eVCU không...
```

## 14. Ghi Chú Triển Khai Trong Source Hiện Tại

Source trong thư mục `COM` đã có các phần có thể tái sử dụng:

- `autosar/can`
- `autosar/canif`
- `autosar/cantp`
- `autosar/com`
- `autosar/pdur`
- `autosar/dcm`
- `config/*_Cfg.c/h`

Tuy nhiên cần sửa:

- `Dcm.c` hiện mới log payload, cần thêm xử lý service UDS và gửi response.
- `CanIf_Cfg.c` cần đặt lại CAN ID diagnostic rõ chiều request/response.
- `Com_Cfg.c` cần cấu hình rõ `EngineStatus` là RX vào eVCU, `VehicleCommand` là TX từ eVCU.
- `Os_Test` cần thêm task cyclic để gọi main function của CAN/CanTp/COM/eVCU.

File ứng dụng eVCU nên tách riêng:

```text
app/Evcu_App.h
app/Evcu_App.c
app/Evcu_Dcm.h
app/Evcu_Dcm.c
app/Evcu_Com.h
app/Evcu_Com.c
```

DCM chỉ nên parse UDS và dispatch. Logic trạng thái xe, DTC, timeout COM nên nằm trong
`Evcu_App`.
