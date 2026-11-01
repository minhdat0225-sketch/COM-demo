/**
 * @file    semihost.c
 * @brief   Semihosting output cho debug qua GDB/OpenOCD.
 * @details Cung cấp retarget _write() để printf()->semihosting.
 *          Chỉ hoạt động khi có debugger kết nối.
 *
 * @version 1.0.0
 * @date    2024-12-11
 * @author  HALA Academy
 */

#include "stm32f10x.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @func    dbg_attached
 * @brief   Kiểm tra có debugger đang kết nối không.
 * @return  int 1 nếu có debugger, 0 nếu không.
 */
static inline int dbg_attached(void) {
  /* Kiểm tra bit C_DEBUGEN trong DHCSR */
  return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0;
}

/**
 * @func    sh_write0
 * @brief   Semihosting SYS_WRITE0 - in chuỗi ra host.
 * @param[in] s Chuỗi kết thúc '\0'.
 */
static inline void sh_write0(const char *s) {
  register uint32_t r0 __asm__("r0") = 0x04; /* SYS_WRITE0 */
  register const char *r1 __asm__("r1") = s; /* con trỏ tới chuỗi */
  __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
}

/**
 * @func    _write
 * @brief   Retarget printf -> semihosting.
 * @details Chỉ hoạt động nếu có debugger. Nếu chạy độc lập sẽ bỏ qua.
 * @param[in] fd   File descriptor (bỏ qua).
 * @param[in] buf  Buffer dữ liệu.
 * @param[in] len  Độ dài dữ liệu.
 * @return  int Số byte đã ghi.
 */
int _write(int fd, const void *buf, size_t len) {
  (void)fd;

  /* Không treo nếu chạy độc lập (không có debugger) */
  if (!dbg_attached()) {
    return (int)len;
  }

  /* Chuyển buffer sang chuỗi tạm + '\0' */
  static char line[256];
  size_t n = (len < sizeof(line) - 1) ? len : (sizeof(line) - 1);
  for (size_t i = 0; i < n; ++i) {
    line[i] = ((const char *)buf)[i];
  }
  line[n] = '\0';
  sh_write0(line);

  return (int)len;
}
