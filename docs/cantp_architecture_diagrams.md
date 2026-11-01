# Sơ đồ Kiến trúc Luồng Dữ liệu CanTp (AUTOSAR COM Stack)

Dựa trên hình ảnh sơ đồ kiến trúc lớp AUTOSAR mà bạn đã cung cấp, tôi đã xây dựng lại 2 sơ đồ Tương đương dành riêng cho luồng **CanTp (Truyền)** và **CanTp (Nhận)**.

Chúng thể hiện rõ cách Dữ liệu (PDU) đi xuyên qua các lớp: *Application -> Service (PduR, CanTp) -> ECU Abstraction (CanIf) -> MCAL (Can) -> Hardware*.

---

## 1. Sơ đồ Luồng CanTp Truyền (CanTp Transmit)

Sơ đồ này mô tả khi Ứng dụng (hoặc DCM) muốn truyền một mảng dữ liệu lớn (Diagnostics) xuống mạng CAN, CanTp sẽ làm nhiệm vụ phân mảnh (Segmentation) từ N-SDU thành các N-PDU nhỏ.

```mermaid
graph TD
    classDef default fill:#f9f9f9,stroke:#333,stroke-width:1px;
    classDef layer fill:#ffffff,stroke:#999,stroke-width:1px,stroke-dasharray: 5 5;
    classDef config fill:#eeeeee,stroke:#666,stroke-width:1px;

    subgraph Layer1 [1. Application Layer / Diagnostics]
        App[main.c / Dcm_Transmit]
    end
    class Layer1 layer

    subgraph Layer2 [2. Service Layer]
        PduR[PduR.c - PDU Router]
        CanTp[CanTp.c - Transport Protocol]
    end
    class Layer2 layer

    subgraph Layer3 [3. ECU Abstraction Layer]
        CanIf[CanIf.c - CAN Interface]
    end
    class Layer3 layer

    subgraph Layer4 [4. MCAL Layer]
        Can[Can.c - CAN Driver]
    end
    class Layer4 layer

    subgraph Layer5 [5. Hardware Layer]
        HW[STM32F103 CAN Hardware - Mailboxes]
    end
    class Layer5 layer

    subgraph Configs [6. Configuration Files]
        CanTpCfg[CanTp_Cfg.c]
        CanIfCfg[CanIf_Cfg.c]
    end
    class Configs layer
    class CanTpCfg,CanIfCfg config

    %% ----- DOWNWARD FLOW (TRANSMISSION) -----
    App -->|1. PduR_DcmTransmit / CanTp_Transmit| PduR
    PduR -->|2. Gọi CanTp_Transmit| CanTp
    CanTp -.->|Tra cứu Config N-SDU / Thông số Timer| CanTpCfg
    
    CanTp -->|3. Polling: CanTp_MainFunction_Tx <br> Cắt nhỏ PDU| CanTp
    CanTp -->|4. CanIf_Transmit| CanIf
    
    CanIf -.->|Tra cứu HTH, CAN ID| CanIfCfg
    CanIf -->|5. Can_Write| Can
    Can -->|6. Ghi thanh ghi Tx Mailbox| HW

    %% ----- UPWARD FLOW (CONFIRMATION) -----
    Can -.->|A. Hardware ngắt / Polling: Can_MainFunction_Write| Can
    Can -->|B. Tx OK -> CanIf_TxConfirmation| CanIf
    CanIf -->|C. Gọi CanTp_TxConfirmation| CanTp
    CanTp -->|D. Gọi PduR_CanTpTxConfirmation| PduR
```

---

## 2. Sơ đồ Luồng CanTp Nhận (CanTp Receive)

Sơ đồ này mô tả khi Mạch (Hardware) bắt được một chuỗi các khung CAN (N-PDU như Single Frame, First Frame, Consecutive Frames), tín hiệu sẽ được truyền theo chiều từ dưới lên để CanTp ghép lại (Reassembly) thành một N-SDU hoàn chỉnh. 

```mermaid
graph TD
    classDef default fill:#f9f9f9,stroke:#333,stroke-width:1px;
    classDef layer fill:#ffffff,stroke:#999,stroke-width:1px,stroke-dasharray: 5 5;
    classDef config fill:#eeeeee,stroke:#666,stroke-width:1px;

    subgraph Layer1 [1. Application Layer / Diagnostics]
        App[main.c / DCM]
    end
    class Layer1 layer

    subgraph Layer2 [2. Service Layer]
        PduR[PduR.c - PDU Router]
        CanTp[CanTp.c - Transport Protocol]
    end
    class Layer2 layer

    subgraph Layer3 [3. ECU Abstraction Layer]
        CanIf[CanIf.c - CAN Interface]
    end
    class Layer3 layer

    subgraph Layer4 [4. MCAL Layer]
        Can[Can.c - CAN Driver]
    end
    class Layer4 layer

    subgraph Layer5 [5. Hardware Layer]
        HW[STM32F103 CAN Hardware - Mailboxes]
    end
    class Layer5 layer

    subgraph Configs [6. Configuration Files]
        CanIfCfg[CanIf_Cfg.c]
        CanTpCfg[CanTp_Cfg.c]
    end
    class Configs layer
    class CanTpCfg,CanIfCfg config

    %% ----- UPWARD RECEIVE FLOW -----
    HW -->|1. Dữ liệu đổ vào Rx Mailbox| Can
    
    Can -->|2. Polling: Can_MainFunction_Read| Can
    Can -->|3. Đọc được Frame -> CanIf_RxIndication| CanIf
    
    CanIf -.->|Phân giải Hrh / CAN ID sang PduId| CanIfCfg
    
    CanIf -->|4. Lọc PduId -> CanTp_RxIndication| CanTp
    
    CanTp -.->|Đối chiếu N-SDU Rx Config| CanTpCfg
    
    CanTp -->|5. Ghép mảnh Payload: CanTp_MainFunction_Rx| CanTp
    
    CanTp -->|6. Hỏi xin App bộ đệm: PduR_CanTpProvideRxBuffer| PduR
    CanTp -->|7. Dữ liệu ghép hoàn chỉnh -> PduR_CanTpRxIndication| PduR
    
    PduR -->|8. Chuyển N-SDU lên App/DCM| App

    %% ----- MỘT SỐ HOẠT ĐỘNG PHỤ (BẮN FLOW CONTROL) -----
    CanTp -.->|9. Trả lời FF bằng: Flow Control Frame <br> Qua CanIf_Transmit| CanIf
```

---
> [!TIP]
> **Điểm chú ý trong CanTp:**
> Không giống như module `Com.c` thông thường chỉ đẩy Data xuống 1 lần là xong. Lớp `CanTp.c` là một "Cỗ máy trạng thái (State Machine)". Do quá trình truyền và nhận bị chia cắt thành nhiều khung CAN nhỏ rải rác theo thời gian, module `CanTp` sẽ dựa dẫm rất nhiều vào sự sống của hàm `CanTp_MainFunction` để cắt/ghép và gửi đi từng mảnh ở mỗi chu kỳ sống (Tick) của chương trình. Đây là lý do ở lượt gỡ lỗi trước, nếu bạn chỉ bấm (F10) 1 lần, hàm MainFunction chưa kịp nhả mảnh tiếp theo, dẫn đến dữ liệu không thể hoàn thiện!
