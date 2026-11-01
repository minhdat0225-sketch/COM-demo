# AUTOSAR COM – Luồng nhận dữ liệu CAN RX (Signal Reception Path)

> **Tài liệu giảng dạy** — HALA Academy  
> **Phiên bản**: 1.0 — Ngày: 2026-04-15  
> **Đối tượng**: Kỹ sư Embedded đã có kiến thức cơ bản về CAN bus  
> **Mục tiêu**: Hiểu trọn vẹn luồng dữ liệu từ khi **CAN frame đến phần cứng** cho đến khi **ứng dụng đọc được giá trị Signal**

---

## Mục lục

1. [Phần 1 – Tổng quan kiến trúc AUTOSAR COM Stack](#phần-1--tổng-quan-kiến-trúc-autosar-com-stack)
2. [Phần 2 – Bốn module trên luồng RX và nhiệm vụ của từng module](#phần-2--bốn-module-trên-luồng-rx-và-nhiệm-vụ-của-từng-module)
3. [Phần 3 – Luồng hoạt động tổng thể (Big Picture)](#phần-3--luồng-hoạt-động-tổng-thể-big-picture)
4. [Phần 4 – Cấu hình tĩnh (Static Configuration)](#phần-4--cấu-hình-tĩnh-static-configuration)
5. [Phần 5 – Triển khai chi tiết từng tầng](#phần-5--triển-khai-chi-tiết-từng-tầng)
6. [Phần 6 – Ứng dụng (Application Layer)](#phần-6--ứng-dụng-application-layer)
7. [Phần 7 – Tổng hợp: Theo dõi 1 CAN frame từ Bus đến Application](#phần-7--tổng-hợp-theo-dõi-1-can-frame-từ-bus-đến-application)
8. [Phần 8 – So sánh luồng TX và RX](#phần-8--so-sánh-luồng-tx-và-rx)
9. [Phần 9 – Bài tập thực hành](#phần-9--bài-tập-thực-hành)

---

## Phần 1 – Tổng quan kiến trúc AUTOSAR COM Stack

### 1.1. AUTOSAR là gì?

**AUTOSAR** (AUTomotive Open System ARchitecture) là một chuẩn kiến trúc phần mềm cho ECU ô tô. Mục tiêu chính: **tách biệt phần mềm ứng dụng khỏi phần cứng**, giúp phần mềm có thể tái sử dụng trên nhiều nền tảng vi điều khiển khác nhau.

### 1.2. Vị trí của COM Stack trong kiến trúc AUTOSAR

Trong AUTOSAR, phần mềm được chia thành 3 lớp chính. COM Stack nằm trải dài từ tầng MCAL đến tầng Services:

```
┌─────────────────────────────────────────────────────┐
│              APPLICATION (SWC)                       │
│         Ứng dụng đọc/ghi Signal                     │
│    Ví dụ: "Đọc RPM động cơ", "Đọc nhiệt độ"       │
├─────────────────────────────────────────────────────┤
│              RTE (Runtime Environment)               │
│     Lớp trung gian, demo này bỏ qua                │
├─────────────────────────────────────────────────────┤
│  ┌──────────┐                                        │
│  │   COM    │  ← Services Layer                      │
│  │          │  Quản lý Signal/I-PDU, pack/unpack     │
│  └────┬─────┘                                        │
│       │                                              │
│  ┌────▼─────┐                                        │
│  │   PduR   │  ← Services Layer                      │
│  │          │  Định tuyến PDU giữa các module        │
│  └────┬─────┘                                        │
│       │                                              │
│  ┌────▼─────┐                                        │
│  │  CanIf   │  ← ECU Abstraction Layer               │
│  │          │  Trừu tượng hóa CAN Driver             │
│  └────┬─────┘                                        │
│       │                                              │
│  ┌────▼─────┐                                        │
│  │   Can    │  ← MCAL (Microcontroller Abstraction)  │
│  │ (bxCAN)  │  Điều khiển trực tiếp phần cứng CAN   │
│  └────┬─────┘                                        │
│       │                                              │
│  ═════▼═══════════════════════════════════════════   │
│       CAN BUS (dây vật lý)                           │
└─────────────────────────────────────────────────────┘
```

### 1.3. Hai khái niệm cốt lõi: Signal vs PDU

AUTOSAR phân biệt rõ hai cấp độ dữ liệu:

| Khái niệm | Mô tả | Ví dụ |
|-----------|-------|-------|
| **Signal** | Một giá trị logic đơn lẻ mà ứng dụng quan tâm | `Engine_RPM = 3500` (16-bit) |
| **I-PDU** | Interaction Layer PDU – một "gói" byte chứa nhiều Signal | `EngineStatus` = 3 byte = \[RPM_Hi, RPM_Lo, Temp\] |
| **L-PDU** | Lower Layer PDU – tương đương CAN frame trên bus | CAN ID 0x181, DLC=3, Data=\[0x0D,0xAC,0x46\] |

**Mối quan hệ**: Nhiều **Signal** được đóng gói (pack) vào một **I-PDU**, rồi I-PDU được đặt vào **L-PDU** (CAN frame) để truyền đi.

```
Signal "RPM" (16-bit)  ──┐
                         ├── pack vào ──► I-PDU "EngineStatus" (3 byte)
Signal "Temp" (8-bit)  ──┘                      │
                                                 ▼
                                     L-PDU (CAN frame 0x181, DLC=3)
```

---

## Phần 2 – Bốn module trên luồng RX và nhiệm vụ của từng module

Trên luồng **nhận** (RX), dữ liệu đi **từ dưới lên trên**. Mỗi tầng có một vai trò rõ ràng:

### 2.1. Can Driver (MCAL) — `Can.c`

| | |
|---|---|
| **Là gì** | Module MCAL điều khiển trực tiếp bộ bxCAN phần cứng của STM32 |
| **Nhiệm vụ RX** | Đọc CAN frame từ FIFO phần cứng, chuyển thành cấu trúc dữ liệu phần mềm |
| **Input** | Tín hiệu điện trên chân PA11 (CAN_RX) |
| **Output** | Gọi `CanIf_RxIndication(canId, &pduInfo)` |
| **Tài liệu AUTOSAR** | SWS_CANDriver (AUTOSAR_SWS_CANDriver.pdf) |

### 2.2. CanIf — CAN Interface — `CanIf.c`

| | |
|---|---|
| **Là gì** | Module ECU Abstraction trừu tượng hóa CAN Driver |
| **Nhiệm vụ RX** | Nhận thông báo từ Can Driver, xác định PDU ID dựa trên CAN ID, chuyển lên PduR |
| **Input** | `CanIf_RxIndication(canId, &pduInfo)` từ Can Driver |
| **Output** | Gọi `PduR_CanIfRxIndication(pduId, &pduInfo)` |
| **Tài liệu AUTOSAR** | SWS_CANInterface (AUTOSAR_SWS_CANInterface.pdf) |

> **Tại sao cần CanIf?** Vì phần cứng CAN chỉ biết CAN ID (ví dụ 0x181), nhưng tầng trên (PduR, COM) làm việc với PDU ID (ví dụ 5). CanIf là nơi dịch CAN ID → PDU ID.

### 2.3. PduR — PDU Router — `PduR.c`

| | |
|---|---|
| **Là gì** | Module Services Layer định tuyến PDU giữa các module |
| **Nhiệm vụ RX** | Nhận PDU từ CanIf, tra bảng route, chuyển đến COM |
| **Input** | `PduR_CanIfRxIndication(canIfRxPduId, &pduInfo)` |
| **Output** | Gọi `Com_RxIndication(comRxPduId, &pduInfo)` |
| **Tài liệu AUTOSAR** | SWS_PDURouter (AUTOSAR_SWS_PDURouter.pdf) |

> **Tại sao cần PduR?** Vì không phải mọi PDU đều đi đến COM. Ví dụ: PDU chẩn đoán (Diagnostic) đi đến DCM, PDU thường đi đến COM. PduR giống như "bưu điện" – nhìn địa chỉ rồi chuyển đúng nơi.

### 2.4. COM — Communication Module — `Com.c`

| | |
|---|---|
| **Là gì** | Module Services Layer quản lý Signal-level communication |
| **Nhiệm vụ RX** | Nhận I-PDU từ PduR, lưu vào shadow buffer. Khi App đọc Signal, giải mã (unpack) byte → giá trị Signal |
| **Input** | `Com_RxIndication(comRxPduId, &pduInfo)` và `Com_ReceiveSignal(signalId, &data)` |
| **Output** | Giá trị Signal trả về cho Application |
| **Tài liệu AUTOSAR** | SWS_COM (AUTOSAR_SWS_COMModule.pdf) |

---

## Phần 3 – Luồng hoạt động tổng thể (Big Picture)

### 3.1. Sơ đồ luồng RX hoàn chỉnh

```
  CAN BUS (tín hiệu điện)
      │
      │  Frame CAN ID=0x181, DLC=3, Data=[0x04,0x1A,0x46]
      │
  ════▼═══════════════════════════════════════════════
  
  ┌──────────────────────────────────────────────────┐
  │  TẦNG 1: Can Driver (MCAL)                      │
  │                                                  │
  │  ISR hoặc Polling:                               │
  │    CAN_Receive(CAN1, FIFO0, &rxMsg)              │
  │    rxMsg.StdId = 0x181                            │
  │    rxMsg.DLC   = 3                                │
  │    rxMsg.Data  = [0x04, 0x1A, 0x46]               │
  │                                                  │
  │  Gọi lên: CanIf_RxIndication(0x181, &pduInfo)    │
  └───────────────────┬──────────────────────────────┘
                      │
  ┌───────────────────▼──────────────────────────────┐  
  │  TẦNG 2: CanIf (ECU Abstraction)                 │
  │                                                  │
  │  Routing theo CAN ID:                            │
  │    if (canId == 0x181)                            │
  │      → PduR_CanIfRxIndication(5, &pduInfo)       │
  │        (5 = CanIfConf_Pdu_EngineStatus)          │
  └───────────────────┬──────────────────────────────┘
                      │
  ┌───────────────────▼──────────────────────────────┐
  │  TẦNG 3: PduR (PDU Router)                       │
  │                                                  │
  │  Tra bảng PduR_CanIfRxRoutes[]:                  │
  │    SrcPduId=5 → DstPduId=5                       │
  │    (CanIfConf_Pdu_EngineStatus                   │  
  │     → ComConf_ComIPdu_EngineStatus)              │
  │                                                  │
  │  Gọi lên: Com_RxIndication(5, &pduInfo)          │
  └───────────────────┬──────────────────────────────┘
                      │
  ┌───────────────────▼──────────────────────────────┐
  │  TẦNG 4: COM (Signal Service)                    │
  │                                                  │
  │  Com_RxIndication():                             │
  │    Tìm buffer của I-PDU ID=5 (EngineStatus)      │
  │    memcpy(shadow_buf, pduInfo.data, 3)            │
  │    → shadow_buf = [0x04, 0x1A, 0x46]             │
  │                                                  │
  │  Khi App gọi Com_ReceiveSignal():                │
  │    RPM  = get_u16(buf, 0) = 0x041A = 1050        │
  │    Temp = get_u8(buf, 2)  = 0x46   = 70          │
  └───────────────────┬──────────────────────────────┘
                      │
  ┌───────────────────▼──────────────────────────────┐
  │  APPLICATION (main)                              │
  │                                                  │
  │  Com_ReceiveSignal(ComSig_Engine_RPM, &rpm);     │
  │  Com_ReceiveSignal(ComSig_Engine_Temp, &temp);   │
  │  → rpm  = 1050                                   │
  │  → temp = 70                                     │
  └──────────────────────────────────────────────────┘
```

### 3.2. Tổng hợp chuỗi hàm gọi (Call Chain)

```
Can_MainFunction_Read()          ← Polling từ main loop
  └─► CAN_Receive()              ← SPL: đọc FIFO phần cứng
  └─► CanIf_RxIndication()       ← Callback lên CanIf
        └─► PduR_CanIfRxIndication()  ← CanIf chuyển lên PduR
              └─► Com_RxIndication()  ← PduR chuyển lên COM (copy vào buffer)

... sau đó, trong vòng lặp main:

Com_ReceiveSignal(SignalId, &data)  ← App đọc Signal từ buffer
  └─► get_u16() / get_u8()          ← Unpack byte → giá trị
```

### 3.3. Hai giai đoạn của luồng RX

Điểm quan trọng mà nhiều người học hay nhầm lẫn: **luồng RX chia làm 2 giai đoạn riêng biệt**:

| Giai đoạn | Trigger | Mô tả |
|----------|---------|-------|
| **Giai đoạn 1: Nhận & Lưu** | ISR hoặc `Can_MainFunction_Read()` | Frame CAN → qua 4 tầng → copy vào shadow buffer |
| **Giai đoạn 2: Đọc Signal** | App gọi `Com_ReceiveSignal()` | App chủ động đọc giá trị từ shadow buffer đã lưu |

Hai giai đoạn này **không đồng bộ** — frame có thể đến bất cứ lúc nào, nhưng App chỉ đọc khi nó muốn.

---

## Phần 4 – Cấu hình tĩnh (Static Configuration)

Trước khi đi vào code triển khai, cần hiểu **cấu hình** — đây là "bản đồ" mà mỗi tầng dựa vào để hoạt động.

### 4.1. Cấu hình COM — Signal & I-PDU Layout

**File**: `config/Com_Cfg.h` + `config/Com_Cfg.c`

#### I-PDU Configuration (bảng Com_IPduCfg)

I-PDU `EngineStatus` (RX) được cấu hình như sau:

```c
// Com_Cfg.c (dành cho Node RX – khi KHÔNG define NODE_TX)
{ .PduId = ComConf_ComIPdu_EngineStatus,   // ID = 5
  .Length = 3u,                             // 3 byte
  .direction = COM_PDU_DIR_RX }             // ← Hướng RX
```

#### Signal Configuration (bảng Com_SignalCfg)

Hai Signal nằm trong I-PDU `EngineStatus`:

```c
// Com_Cfg.c
[ComSig_Engine_RPM] = {
    .PduId     = ComConf_ComIPdu_EngineStatus,  // Thuộc I-PDU nào
    .byteIndex = 0u,      // Bắt đầu từ byte 0
    .bitOffset = 0u,      // Bit offset = 0
    .bitLength = 16u,     // Độ dài 16-bit (2 byte)
    .type      = COM_SIGTYPE_UINT8,
    .direction = COM_PDU_DIR_RX
},
[ComSig_Engine_Temp] = {
    .PduId     = ComConf_ComIPdu_EngineStatus,
    .byteIndex = 2u,      // Bắt đầu từ byte 2
    .bitOffset = 0u,
    .bitLength = 8u,      // Độ dài 8-bit (1 byte)
    .type      = COM_SIGTYPE_UINT8,
    .direction = COM_PDU_DIR_RX
}
```

Minh họa layout trong I-PDU `EngineStatus` (3 byte):

```
Byte:    [  0  ] [  1  ] [  2  ]
         ├──────────────┤├─────┤
Signal:  │  Engine_RPM  ││Temp │
         │  (16-bit BE) ││(8b) │
         └──────────────┘└─────┘
```

### 4.2. Cấu hình CanIf — Mapping CAN ID → PDU ID

**File**: `config/CanIf_Cfg.h` + `config/CanIf_Cfg.c`

CanIf sử dụng bảng `CanIf_TxPduCfg` (cũng dùng chung cho lookup RX theo CAN ID):

```c
// CanIf_Cfg.h — Định danh symbolic
enum {
    CanIfConf_Pdu_EngineCmd    = 0u,   // CAN 0x180
    CanIfConf_Pdu_BrakeCmd     = 1u,   // CAN 0x280
    CanIfConf_Pdu_BodyCmd      = 2u,   // CAN 0x380
    CanIfConf_Pdu_DiagTx       = 3u,   // CAN 0x7DF
    CanIfConf_Pdu_DiagRx_FC    = 4u,   // CAN 0x7E8
    CanIfConf_Pdu_EngineStatus = 5u    // CAN 0x181 ← PDU cho luồng RX
};

// CanIf_Cfg.c — Bảng ánh xạ
{ .CanIfTxPduId = CanIfConf_Pdu_EngineStatus,
  .Hth = 5u,
  .CanId = 0x181u,      // ← CAN ID trên bus
  .DlcMax = 3u }
```

> **Điểm mấu chốt**: Khi CanIf nhận CAN ID `0x181`, nó biết đây là PDU `CanIfConf_Pdu_EngineStatus` (ID = 5).

### 4.3. Cấu hình PduR — Routing Table

**File**: `config/PduR_Cfg.h` + `config/PduR_Cfg.c`

PduR dùng bảng `PduR_CanIfRxRoutes` để biết PDU từ CanIf sẽ đi đến COM ID nào:

```c
// PduR_Cfg.c
const PduR_CallbackRouteType PduR_CanIfRxRoutes[1] = {
    { .SrcPduId = CanIfConf_Pdu_EngineStatus,     // = 5 (từ CanIf)
      .DstPduId = ComConf_ComIPdu_EngineStatus }   // = 5 (đến COM)
};
```

> **Trong demo này** SrcPduId và DstPduId trùng nhau (đều = 5), nhưng trong hệ thống thực tế chúng **có thể khác nhau** vì mỗi module có không gian ID riêng.

### 4.4. Tổng hợp cấu hình — Ánh xạ xuyên suốt

```
CAN ID (phần cứng)    PDU ID (CanIf)         PDU ID (COM)           Signal ID
      0x181        →  CanIfConf_Pdu_         → ComConf_ComIPdu_      → ComSig_Engine_RPM (17)
                      EngineStatus (5)         EngineStatus (5)       → ComSig_Engine_Temp (18)
```

---

## Phần 5 – Triển khai chi tiết từng tầng

### 5.1. Tầng 1 — Can Driver: Đọc frame từ phần cứng

**File**: `autosar/can/Can.c`

#### 5.1.1. Cơ chế nhận: ISR + Polling

CAN Driver hỗ trợ **2 cơ chế nhận** song song:

```
CAN frame đến bus
      │
      ▼
  bxCAN Hardware (FIFO0)
      │
      ├──► ISR: USB_LP_CAN1_RX0_IRQHandler()   ← Tức thì (interrupt)
      │
      └──► Polling: Can_MainFunction_Read()      ← Định kỳ (backup)
```

#### 5.1.2. ISR — Nhận tức thì qua Interrupt

```c
// Can.c — dòng 404-417
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    CanRxMsg rxMsg;       // Cấu trúc SPL chứa CAN frame
    PduInfoType pduInfo;  // Cấu trúc AUTOSAR chứa PDU

    // Vòng lặp đọc hết tất cả frame trong FIFO0
    while (CAN_MessagePending(CAN_PERIPH, CAN_FIFO0) > 0) {
        // Bước 1: Đọc frame từ FIFO phần cứng
        CAN_Receive(CAN_PERIPH, CAN_FIFO0, &rxMsg);

        // Bước 2: Chuyển đổi SPL → AUTOSAR
        pduInfo.SduDataPtr = rxMsg.Data;   // Con trỏ đến payload
        pduInfo.SduLength  = rxMsg.DLC;    // Số byte payload

        // Bước 3: Gọi callback lên tầng trên (CanIf)
        CanIf_RxIndication(rxMsg.StdId, &pduInfo);
    }
}
```

**Giải thích từng bước**:

1. `CAN_MessagePending()` — Hàm SPL kiểm tra có bao nhiêu frame đang chờ trong FIFO
2. `CAN_Receive()` — Hàm SPL đọc 1 frame ra khỏi FIFO, lưu vào `rxMsg`
3. **Chuyển đổi kiểu dữ liệu**: `CanRxMsg` (kiểu SPL) → `PduInfoType` (kiểu AUTOSAR)
4. `CanIf_RxIndication()` — Gọi lên tầng trên, truyền CAN ID và payload

#### 5.1.3. Polling — Backup qua Main Function

```c
// Can.c — dòng 370-396
void Can_MainFunction_Read(void)
{
    if (!s_inited) return;

    CanRxMsg rxMsg;
    PduInfoType pduInfo;
    
    // Kiểm tra FIFO0
    while (CAN_MessagePending(CAN_PERIPH, CAN_FIFO0) > 0) {
        CAN_Receive(CAN_PERIPH, CAN_FIFO0, &rxMsg);
        pduInfo.SduDataPtr = rxMsg.Data;
        pduInfo.SduLength  = rxMsg.DLC;
        CanIf_RxIndication(rxMsg.StdId, &pduInfo);
    }
    
    // Kiểm tra FIFO1 (tương tự)
    while (CAN_MessagePending(CAN_PERIPH, CAN_FIFO1) > 0) { ... }
}
```

> **Câu hỏi**: Tại sao cần cả ISR lẫn Polling?  
> **Trả lời**: ISR xử lý nhanh, nhưng trên emulator (Renode) đôi khi interrupt không trigger đúng. Polling là "lưới an toàn" — được gọi định kỳ từ `main()` để đảm bảo không bỏ sót frame.

### 5.2. Tầng 2 — CanIf: Dịch CAN ID → PDU ID

**File**: `autosar/canif/CanIf.c`

```c
// CanIf.c — dòng 162-178
void CanIf_RxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    if (PduInfoPtr == NULL) return;
    
    // Ghi log debug (circular buffer)
    CanIf_LastRxIds[CanIf_RxIdIndex % 10] = canId;
    CanIf_RxIdIndex++;

    // Routing theo CAN ID
    if (canId == 0x7DF || canId == 0x7E8) {
        // CAN ID chẩn đoán → chuyển cho CanTp (Transport Protocol)
        CanTp_RxIndication(0, PduInfoPtr);
    }
    else if (canId == 0x181) {
        // CAN ID = 0x181 → đây là EngineStatus
        // Chuyển lên PduR với PDU ID = CanIfConf_Pdu_EngineStatus (5)
        PduR_CanIfRxIndication(CanIfConf_Pdu_EngineStatus, PduInfoPtr);
    }
    // Các CAN ID khác bị bỏ qua (không có route)
}
```

**Phân tích logic routing**:

```
canId nhận được
      │
      ├── 0x7DF, 0x7E8 → CanTp (Diagnostic path, không thuộc scope bài này)
      │
      ├── 0x181 ────────► PduR_CanIfRxIndication(5, &pduInfo)
      │                   (EngineStatus → luồng COM RX)
      │
      └── Khác ─────────► Bỏ qua (không có route)
```

> **Lưu ý quan trọng**: Trong sản phẩm thật, logic routing này sẽ dùng **bảng cấu hình** (tra bảng `CanIf_RxPduCfg`) thay vì hard-code `if/else`. Demo đơn giản hóa để dễ hiểu.

### 5.3. Tầng 3 — PduR: Định tuyến đến đúng module đích

**File**: `autosar/pdur/PduR.c`

```c
// PduR.c — dòng 182-194
void PduR_CanIfRxIndication(PduIdType CanIfRxPduId, const PduInfoType* PduInfoPtr)
{
    // Kiểm tra trạng thái: PduR đã init và routing đang bật
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return;
    if ((s_Cfg == NULL) || (s_Cfg->CanIfRxRoutingTable == NULL)) return;

    // Tra bảng route RX: tìm entry có SrcPduId khớp
    int32_t idx = prv_find_callback_route(
        (const PduR_CallbackRouteType*)s_Cfg->CanIfRxRoutingTable,
        PDUR_NUM_CANIF_RX_ROUTES,   // = 1 (chỉ có 1 route)
        CanIfRxPduId                // = 5 (CanIfConf_Pdu_EngineStatus)
    );

    if (idx >= 0) {
        // Route tìm thấy! Chuyển lên COM với DstPduId
        PduIdType comPduId = ((const PduR_CallbackRouteType*)
                              s_Cfg->CanIfRxRoutingTable)[idx].DstPduId;
        Com_RxIndication(comPduId, PduInfoPtr);
        // comPduId = 5 (ComConf_ComIPdu_EngineStatus)
    }
}
```

**Bảng route mà PduR tra cứu**:

```
PduR_CanIfRxRoutes[] (config/PduR_Cfg.c):
┌───────┬────────────────────────────┬────────────────────────────────┐
│ Index │ SrcPduId (từ CanIf)        │ DstPduId (đến COM)             │
├───────┼────────────────────────────┼────────────────────────────────┤
│   0   │ 5 (CanIfConf_Pdu_Engine-   │ 5 (ComConf_ComIPdu_Engine-     │
│       │      Status)               │      Status)                   │
└───────┴────────────────────────────┴────────────────────────────────┘
```

**Hàm helper tra bảng**:
```c
// PduR.c — dòng 72-81
static inline int32_t prv_find_callback_route(
    const PduR_CallbackRouteType* tbl,
    uint16 n,          // Số lượng entry
    PduIdType src)     // PDU ID cần tìm
{
    for (uint16 i = 0; i < n; ++i) {
        if (tbl[i].SrcPduId == src) {
            return (int32_t)i;  // Tìm thấy! Trả về index
        }
    }
    return -1;  // Không tìm thấy
}
```

### 5.4. Tầng 4 — COM: Lưu vào buffer & Unpack Signal

**File**: `autosar/com/Com.c`

#### 5.4.1. Giai đoạn 1: Com_RxIndication — Copy dữ liệu vào shadow buffer

```c
// Com.c — dòng 302-318
void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) {
        return;
    }

    // Tìm buffer và kiểm tra hướng
    PduLengthType len = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_TX;
    uint8* buf = prv_get_ipdu_buf(ComRxPduId, &len, &dir);

    if ((buf != NULL) && (dir == COM_PDU_DIR_RX)) {
        // Copy payload từ CAN bus vào shadow buffer
        PduLengthType copyLen = (PduInfoPtr->SduLength < len) 
                                 ? PduInfoPtr->SduLength : len;
        memcpy(buf, PduInfoPtr->SduDataPtr, copyLen);
    }
}
```

**Minh họa bộ nhớ**:

```
Trước Com_RxIndication():
  Com_IpduBuf[5] = [0x00, 0x00, 0x00]  ← Giá trị khởi tạo (toàn 0)

Sau Com_RxIndication(5, {data=[0x04,0x1A,0x46], len=3}):
  Com_IpduBuf[5] = [0x04, 0x1A, 0x46]  ← Copy từ CAN frame
                    ├──────────┤ ├──┤
                    RPM=0x041A   Temp=0x46
                    RPM=1050     Temp=70
```

#### 5.4.2. Giai đoạn 2: Com_ReceiveSignal — Unpack từ buffer ra Signal

```c
// Com.c — dòng 254-296
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if (SignalDataPtr == NULL) return COM_SERVICE_NOT_AVAILABLE;
    if (SignalId >= COM_NUM_SIGNALS) return COM_SERVICE_NOT_AVAILABLE;

    // Lấy cấu hình Signal: thuộc PDU nào, byte nào, bao nhiêu bit
    const Com_SignalCfgType* cfg = &Com_SignalCfg[SignalId];

    // Tìm buffer của I-PDU chứa signal này
    PduLengthType ipduLen = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_TX;
    uint8* ipdu = prv_get_ipdu_buf(cfg->PduId, &ipduLen, &dir);

    // Kiểm tra: buffer hợp lệ + hướng phải là RX
    if ((ipdu == NULL) || (dir != COM_PDU_DIR_RX) || 
        (cfg->byteIndex >= ipduLen)) {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    // Unpack theo độ dài bit được cấu hình
    switch (cfg->bitLength) {
        case 16u:
            *(uint16*)SignalDataPtr = get_u16(ipdu, cfg->byteIndex);
            break;
        case 8u:
            if (cfg->type == COM_SIGTYPE_BOOLEAN) {
                *(boolean*)SignalDataPtr = (get_u8(ipdu, cfg->byteIndex) != 0u);
            } else {
                *(uint8*)SignalDataPtr = get_u8(ipdu, cfg->byteIndex);
            }
            break;
        case 1u:
            *(boolean*)SignalDataPtr = get_bit(ipdu, cfg->byteIndex, 
                                               cfg->bitOffset);
            break;
        default:
            return COM_SERVICE_NOT_AVAILABLE;
    }

    return E_OK;
}
```

**Hàm unpack helper — Giải mã 16-bit Big Endian**:

```c
// Com.c — dòng 129-132
static inline uint16 get_u16(const uint8* src, uint16 byteIndex)
{
    // Big Endian: byte cao trước, byte thấp sau
    return (uint16)(((uint16)src[byteIndex] << 8) | 
                     (uint16)src[byteIndex + 1]);
}
```

**Ví dụ cụ thể**:
```
buffer = [0x04, 0x1A, 0x46]
                         
get_u16(buf, 0) = (0x04 << 8) | 0x1A = 0x041A = 1050 (RPM)
get_u8(buf, 2)  = 0x46 = 70 (Temp)
```

---

## Phần 6 – Ứng dụng (Application Layer)

**File**: `example/can_rx_demo.c`

### 6.1. Khởi tạo BSW Stack

```c
// can_rx_demo.c — dòng 50-55
void BSW_Init_RX(void) {
    Can_Init();                // 1. Khởi tạo phần cứng bxCAN
    CanIf_Init();              // 2. Khởi tạo CanIf (đánh dấu sẵn sàng)
    PduR_Init(&PduR_ConfigPB); // 3. Khởi tạo PduR (load bảng route)
    Com_Init();                // 4. Khởi tạo COM (xóa shadow buffer)
}
```

**Thứ tự khởi tạo rất quan trọng**: Phải init từ **dưới lên trên** (Can → CanIf → PduR → COM), vì tầng trên phụ thuộc vào tầng dưới.

### 6.2. Vòng lặp chính — Nhận và đọc Signal

```c
// can_rx_demo.c — dòng 57-90
int main(void)
{
    Log_Init();   // Khởi tạo UART để in log debug
    Log_Print("\r\n--- VCU ECU (Node RX) Booted ---\r\n");

    BSW_Init_RX();  // Khởi tạo toàn bộ BSW Stack

    uint16 rpm = 0;
    uint8 temp = 0;
    uint16 last_rpm = 0xFFFF;  // Để phát hiện thay đổi

    while (1) {
        // ──── GIAI ĐOẠN 1: Kích hoạt nhận ────
        // Polling CAN Driver để kiểm tra frame mới
        // (ISR cũng đang chạy song song, nhưng đây là backup)
        Can_MainFunction_Read();
        // → Nếu có frame → chuỗi callback chạy tự động:
        //   CanIf_RxIndication → PduR_CanIfRxIndication → Com_RxIndication
        //   → Data đã nằm trong shadow buffer

        // ──── GIAI ĐOẠN 2: Đọc Signal từ buffer ────
        if (Com_ReceiveSignal(ComSig_Engine_RPM, &rpm) == E_OK) {
            // Chỉ in log khi giá trị thay đổi
            if (rpm != last_rpm) {
                Com_ReceiveSignal(ComSig_Engine_Temp, &temp);
                
                char msg[80];
                sprintf(msg, "RX: Received EngineStatus -> RPM=%u, Temp=%u\r\n",
                        rpm, temp);
                Log_Print(msg);
                
                last_rpm = rpm;
            }
        }

        delay_ms(10u);  // Chu kỳ polling 10ms
    }
}
```

### 6.3. Tại sao App không biết gì về CAN?

Nhìn vào code Application, ta thấy:

- ❌ Không có `CAN_Receive()`
- ❌ Không có CAN ID `0x181`  
- ❌ Không có `rxMsg.Data[0]`

Thay vào đó, App chỉ dùng:

- ✅ `Com_ReceiveSignal(ComSig_Engine_RPM, &rpm)` — Đọc tín hiệu theo tên

**Đây chính là mục tiêu của AUTOSAR**: Application chỉ quan tâm đến "giá trị Signal", không cần biết Signal đó đến từ CAN, LIN, FlexRay hay Ethernet.

---

## Phần 7 – Tổng hợp: Theo dõi 1 CAN frame từ Bus đến Application

Giả sử Node TX gửi `RPM=1050, Temp=70`:

### Bước 1: TX Node pack và gửi
```
Com_SendSignal(RPM, &1050)  → put_u16(buf, 0, 1050)
                              buf[0]=0x04, buf[1]=0x1A
Com_SendSignal(Temp, &70)   → put_u8(buf, 2, 70)
                              buf[2]=0x46

Com_TriggerIPDUSend(5) → PduR → CanIf → Can_Write()
                         → CAN frame: ID=0x181, DLC=3, [04,1A,46]
```

### Bước 2: Frame truyền trên CAN Bus
```
Engine_ECU ──CAN_TX──►  ════CAN BUS════  ──CAN_RX──► VCU_ECU
                         ID=0x181
                         [0x04, 0x1A, 0x46]
```

### Bước 3: RX Node nhận và lưu
```
bxCAN Hardware (VCU_ECU)
  → FIFO0 có frame mới
  → ISR/Polling đọc: rxMsg = {StdId=0x181, DLC=3, Data=[04,1A,46]}
  
  → CanIf_RxIndication(0x181, {ptr=[04,1A,46], len=3})
      canId == 0x181  → PduR_CanIfRxIndication(5, ...)
  
  → PduR_CanIfRxIndication(5, {ptr=[04,1A,46], len=3})
      Tra bảng: SrcPduId=5 → DstPduId=5
      → Com_RxIndication(5, ...)
  
  → Com_RxIndication(5, {ptr=[04,1A,46], len=3})
      memcpy(Com_IpduBuf[5], [04,1A,46], 3)
```

### Bước 4: App đọc Signal
```
Com_ReceiveSignal(17, &rpm)
  → SignalCfg[17]: PduId=5, byteIndex=0, bitLength=16
  → buffer = Com_IpduBuf[5] = [04, 1A, 46]
  → rpm = get_u16(buf, 0) = (0x04<<8)|0x1A = 1050 ✓

Com_ReceiveSignal(18, &temp)
  → SignalCfg[18]: PduId=5, byteIndex=2, bitLength=8
  → temp = get_u8(buf, 2) = 0x46 = 70 ✓
```

---

## Phần 8 – So sánh luồng TX và RX

| Đặc điểm | Luồng TX (Gửi) | Luồng RX (Nhận) |
|----------|----------------|-----------------|
| **Hướng dữ liệu** | Trên → Dưới (App → Bus) | Dưới → Trên (Bus → App) |
| **Trigger** | App gọi `Com_SendSignal()` | ISR hoặc `Can_MainFunction_Read()` |
| **COM làm gì** | Pack Signal → I-PDU buffer | Copy I-PDU vào buffer + Unpack khi đọc |
| **PduR làm gì** | Tra bảng TX route → gọi CanIf | Tra bảng RX route → gọi COM |
| **CanIf làm gì** | Tra bảng TX config → gọi Can_Write | Dịch CAN ID → PDU ID → gọi PduR |
| **Can Driver** | Nạp frame vào mailbox TX | Đọc frame từ FIFO RX |
| **Cơ chế gọi** | API call (đồng bộ) | Callback chain (bất đồng bộ từ ISR) |

### Sơ đồ so sánh trực quan:

```
LUỒNG TX:                        LUỒNG RX:
                                  
App                               App
 │ Com_SendSignal()                ▲ Com_ReceiveSignal()
 ▼                                │
COM (pack → buffer)              COM (buffer → unpack)
 │ Com_TriggerIPDUSend()          ▲ Com_RxIndication()
 ▼                                │
PduR (tra route TX)              PduR (tra route RX)
 │ PduR_ComTransmit()             ▲ PduR_CanIfRxIndication()
 ▼                                │
CanIf (tra config TX)            CanIf (CAN ID → PDU ID)
 │ CanIf_Transmit()               ▲ CanIf_RxIndication()
 ▼                                │
Can Driver                       Can Driver
 │ Can_Write()                    ▲ Can_MainFunction_Read() / ISR
 ▼                                │
═══CAN BUS═══                ═══CAN BUS═══
```

---

## Phần 9 – Bài tập thực hành

### Bài 1: Thêm Signal mới (Cơ bản)

Thêm Signal `Engine_OilPressure` (8-bit, byte index 3) vào I-PDU `EngineStatus`.

**Việc cần làm**:
1. Sửa `Com_Cfg.h`: Thêm `ComSig_Engine_OilPressure = 19u`, tăng `COM_NUM_SIGNALS` lên 20
2. Sửa `Com_Cfg.c`: Thêm entry vào `Com_SignalCfg[]`
3. Sửa `Com_Cfg.c`: Tăng `Length` của `EngineStatus` từ 3 lên 4
4. Sửa `CanIf_Cfg.c`: Tăng `DlcMax` từ 3 lên 4
5. Sửa `can_tx_demo.c`: Gửi thêm `Com_SendSignal(ComSig_Engine_OilPressure, &oil)`
6. Sửa `can_rx_demo.c`: Đọc thêm `Com_ReceiveSignal(ComSig_Engine_OilPressure, &oil)`

### Bài 2: Thêm RX PDU mới (Nâng cao)

Thêm I-PDU `BrakeStatus` (CAN ID 0x282, DLC=2) chứa Signal `BrakePedal` (8-bit) và `BrakeForce` (8-bit).

**Việc cần làm**:
1. Thêm ID vào `Com_Cfg.h`, `CanIf_Cfg.h`
2. Thêm cấu hình vào `Com_Cfg.c`, `CanIf_Cfg.c`, `PduR_Cfg.c`
3. Thêm routing trong `CanIf_RxIndication()` cho CAN ID 0x282
4. Tăng `COM_NUM_IPDUS`, `COM_NUM_SIGNALS`, `PDUR_NUM_CANIF_RX_ROUTES`

### Bài 3: Trace & Debug (Phân tích)

Biên dịch và chạy trên Renode, sau đó trả lời:
1. Nếu Node TX gửi RPM=8000, dữ liệu byte trên CAN bus là gì?
2. Nếu bỏ route trong `PduR_CanIfRxRoutes`, Node RX có nhận được gì không? Tại sao?
3. Nếu đổi `direction` của I-PDU từ `COM_PDU_DIR_RX` thành `COM_PDU_DIR_TX`, điều gì xảy ra?

---

## Tham khảo

| Tài liệu | Mô tả |
|----------|-------|
| AUTOSAR_SWS_COMModule.pdf | Specification of Communication Module |
| AUTOSAR_SWS_PDURouter.pdf | Specification of PDU Router |
| AUTOSAR_SWS_CANInterface.pdf | Specification of CAN Interface |
| AUTOSAR_SWS_CANDriver.pdf | Specification of CAN Driver |
| [autosar.org](https://www.autosar.org) | Tải tài liệu chính thức |

---

> **Ghi chú cho giảng viên**: Tài liệu này sử dụng source code thực tế từ dự án demo đã chạy thành công trên Renode emulator. Tất cả code trích dẫn đều tham chiếu đến file và dòng cụ thể trong project, giúp học viên có thể đối chiếu trực tiếp.
