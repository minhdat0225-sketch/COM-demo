#ifndef STD_TYPES_H
#define STD_TYPES_H

/**
 * @file Std_Types.h
 * @brief Các kiểu dữ liệu chuẩn cho AUTOSAR
 *
 * File này cung cấp các định nghĩa kiểu chuẩn được sử dụng trong toàn bộ stack
 * AUTOSAR.
 *
 * @version 1.0.0
 * @date    2025-10-15
 * @author  HALA Academy
 */

#include <stdint.h>

#ifndef FALSE
#define FALSE (0u)
#endif

#ifndef TRUE
#define TRUE (1u)
#endif

#ifndef STD_ON
#define STD_ON (1u)
#endif

#ifndef STD_OFF
#define STD_OFF (0u)
#endif

/* Kiểu trả về chuẩn */
typedef uint8_t Std_ReturnType;

#define E_OK ((Std_ReturnType)0x00u)
#define E_NOT_OK ((Std_ReturnType)0x01u)

/* Kiểu Boolean */
typedef uint8_t boolean;

/* Các kiểu chuẩn */
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float float32;
typedef double float64;

#endif /* STD_TYPES_H */
