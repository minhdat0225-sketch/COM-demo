/**********************************************************
 * @file    Can.h
 * @brief   AUTOSAR CAN Driver – bxCAN nội bộ STM32F103
 *
 * @details Module MCAL (Microcontroller Abstraction Layer) điều
 *          khiển bộ CAN tích hợp sẵn trong vi điều khiển STM32F103.
 *
 *          STM32F103 có bxCAN (Basic Extended CAN) tại địa chỉ
 *          0x40006400 với các đặc điểm:
 *            - Hỗ trợ CAN 2.0A (Standard 11-bit ID) và 2.0B (Extended 29-bit ID)
 *            - 3 mailbox truyền (TX Mailbox 0, 1, 2)
 *            - 2 FIFO nhận (FIFO0, FIFO1) mỗi FIFO 3 cấp
 *            - 14 bộ lọc (Filter Bank) có thể cấu hình
 *            - Tốc độ tối đa 1 Mbps
 *
 *          Chân phần cứng:
 *            PA11 = CAN_RX (nhận dữ liệu từ CAN bus)
 *            PA12 = CAN_TX (truyền dữ liệu lên CAN bus)
 *
 *          API cung cấp (chuẩn AUTOSAR):
 *            Can_Init()               : Cấu hình GPIO + bxCAN peripheral
 *            Can_Write()              : Gửi CAN frame qua bxCAN mailbox
 *            Can_MainFunction_Write() : Kiểm tra TX hoàn tất (polling)
 *
 * @version 2.0
 * @date    2025-09-19
 * @author  HALA Academy
 **********************************************************/
#ifndef CAN_H
#define CAN_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Khởi tạo CAN Driver
 * @details Thực hiện các bước:
 *          1. Bật clock cho GPIOA, AFIO, CAN1
 *          2. Cấu hình GPIO: PA12=CAN_TX (AF Push-Pull), PA11=CAN_RX (Input)
 *          3. Cấu hình bxCAN: tốc độ 500kbps, LoopBack mode
 *          4. Cấu hình bộ lọc: Filter 0 chấp nhận tất cả CAN ID
 *          5. Clear các biến trạng thái TX
 *
 * @pre     Hệ thống clock (RCC) đã được cấu hình (SystemInit)
 * @post    CAN Driver sẵn sàng nhận lệnh Can_Write()
 */
void Can_Init(void);

/**
 * @brief   Gửi 1 CAN PDU qua bxCAN
 * @param   Hth      Hardware Transmit Handle – chọn slot TX (0..2)
 * @param   PduInfo  Con trỏ tới cấu trúc Can_PduType chứa:
 *                   - id          : CAN ID (Standard hoặc Extended)
 *                   - length      : DLC (0..8 byte)
 *                   - sdu         : Con trỏ tới dữ liệu
 *                   - swPduHandle : Handle để callback xác nhận
 * @return  CAN_OK     : Đã nạp vào mailbox, đang truyền
 *          CAN_BUSY   : Tất cả mailbox TX đang bận
 *          CAN_NOT_OK : Lỗi tham số (NULL pointer, chưa init...)
 */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);

/**
 * @brief   Hàm polling kiểm tra TX hoàn tất
 * @details Gọi định kỳ trong vòng lặp chính (main loop) hoặc từ OS task.
 *          Kiểm tra trạng thái các mailbox TX:
 *          - Nếu TX thành công → gọi CanIf_TxConfirmation() báo lên tầng trên
 *          - Nếu TX thất bại  → clear trạng thái pending
 *          - Nếu TX đang chờ  → không làm gì (chờ tiếp)
 */
void Can_MainFunction_Write(void);

/**
 * @brief   Hàm polling kiểm tra RX (backup cho ISR)
 * @details Gọi định kỳ trong vòng lặp chính.
 *          Kiểm tra FIFO0/FIFO1 xem có message pending không.
 *          Nếu có → đọc ra và gọi CanIf_RxIndication().
 *          Lưu ý: Trên Renode, RX chính xử lý qua ISR
 *          (USB_LP_CAN1_RX0_IRQHandler).
 */
void Can_MainFunction_Read(void);

#ifdef __cplusplus
}
#endif
#endif /* CAN_H */
