/**********************************************************
 * @file    Std_Types.h
 * @brief   Kiểu dữ liệu chuẩn AUTOSAR (AUTOSAR Standard Types)
 *
 * @details File này cung cấp các định nghĩa kiểu dữ liệu nền tảng
 *          được sử dụng xuyên suốt toàn bộ AUTOSAR BSW stack:
 *
 *          1. Kiểu số nguyên có kích thước cố định:
 *             uint8, uint16, uint32, uint64, int8, int16, int32, int64
 *
 *          2. Kiểu boolean:
 *             boolean (TRUE/FALSE)
 *
 *          3. Kiểu trả về chuẩn:
 *             Std_ReturnType (E_OK / E_NOT_OK)
 *
 *          4. Kiểu số thực:
 *             float32, float64
 *
 *          Tham chiếu: AUTOSAR_SWS_StandardTypes (R4.x)
 *
 * @version 1.0.0
 * @date    2025-10-15
 * @author  HALA Academy
 **********************************************************/

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

/* ===========================================================
 * Hằng số Boolean
 * -----------------------------------------------------------
 * Định nghĩa TRUE/FALSE nếu chưa có từ thư viện hệ thống.
 * Trong AUTOSAR, boolean là kiểu 8-bit (uint8_t).
 * ===========================================================*/
#ifndef FALSE
#define FALSE (0u)
#endif

#ifndef TRUE
#define TRUE (1u)
#endif

/* ===========================================================
 * Trạng thái Module (STD_ON / STD_OFF)
 * -----------------------------------------------------------
 * Dùng trong cấu hình biên dịch (#if STD_ON == xxx)
 * để bật/tắt tính năng tại thời điểm compile.
 * ===========================================================*/
#ifndef STD_ON
#define STD_ON (1u)
#endif

#ifndef STD_OFF
#define STD_OFF (0u)
#endif

/* ===========================================================
 * Kiểu trả về chuẩn (Std_ReturnType)
 * -----------------------------------------------------------
 * E_OK     (0x00): Thành công
 * E_NOT_OK (0x01): Thất bại / lỗi chung
 *
 * Mọi API BSW đều dùng kiểu này để trả kết quả.
 * ===========================================================*/
typedef uint8_t Std_ReturnType;

#define E_OK     ((Std_ReturnType)0x00u)
#define E_NOT_OK ((Std_ReturnType)0x01u)

/* ===========================================================
 * Kiểu Boolean (boolean)
 * -----------------------------------------------------------
 * 8-bit, giá trị: TRUE (1) hoặc FALSE (0).
 * Dùng thay thế cho _Bool của C99 để đảm bảo tương thích
 * trình biên dịch trên các nền tảng nhúng khác nhau.
 * ===========================================================*/
typedef uint8_t boolean;

/* ===========================================================
 * Kiểu số nguyên có kích thước cố định
 * -----------------------------------------------------------
 * Tương ứng với <stdint.h> nhưng dùng tên ngắn hơn
 * theo quy ước AUTOSAR Platform Types.
 * ===========================================================*/
typedef uint8_t   uint8;      /**< Số nguyên không dấu 8-bit  (0..255)        */
typedef uint16_t  uint16;     /**< Số nguyên không dấu 16-bit (0..65535)      */
typedef uint32_t  uint32;     /**< Số nguyên không dấu 32-bit (0..4294967295) */
typedef uint64_t  uint64;     /**< Số nguyên không dấu 64-bit                 */

typedef int8_t    int8;       /**< Số nguyên có dấu 8-bit  (-128..127)        */
typedef int16_t   int16;      /**< Số nguyên có dấu 16-bit (-32768..32767)    */
typedef int32_t   int32;      /**< Số nguyên có dấu 32-bit                    */
typedef int64_t   int64;      /**< Số nguyên có dấu 64-bit                    */

/* ===========================================================
 * Kiểu số thực dấu phẩy động
 * ===========================================================*/
typedef float     float32;    /**< Số thực 32-bit (IEEE 754 single precision)  */
typedef double    float64;    /**< Số thực 64-bit (IEEE 754 double precision)  */

#endif /* STD_TYPES_H */
