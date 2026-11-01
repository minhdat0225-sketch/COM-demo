/**
 * @file    syscalls_min.c
 * @brief   Minimal syscalls cho newlib/embedded.
 * @details Cung cấp các stubs cho malloc/sbrk và các syscall cơ bản.
 *          _write() được định nghĩa trong semihost.c.
 *
 * @version 1.0.0
 * @date    2024-12-11
 * @author  HALA Academy
 */

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ==========================================================================
 *                          HEAP/STACK SYMBOLS
 * ========================================================================== */

/** @brief Các symbol từ linker script */
extern char __HeapLimit __attribute__((weak));
extern char __bss_end__ __attribute__((weak));
extern char _ebss __attribute__((weak));
extern char end __attribute__((weak));
extern char _end __attribute__((weak));
extern char _estack; /**< Top of stack (phải có trong linker) */

/** @brief Con trỏ heap hiện tại */
static char *heap_end = 0;

/**
 * @func    get_heap_base
 * @brief   Lấy địa chỉ bắt đầu của heap.
 * @return  char* Địa chỉ heap base hoặc 0 nếu không tìm thấy.
 */
static inline char *get_heap_base(void) {
  if (&__HeapLimit)
    return &__HeapLimit;
  if (&__bss_end__)
    return &__bss_end__;
  if (&_ebss)
    return &_ebss;
  if (&end)
    return &end;
  if (&_end)
    return &_end;
  return 0;
}

/* ==========================================================================
 *                          SYSCALL IMPLEMENTATIONS
 * ========================================================================== */

/**
 * @func    _sbrk
 * @brief   Tăng/giảm heap cho malloc.
 * @param[in] incr Số byte cần tăng thêm.
 * @return  void* Con trỏ tới vùng mới hoặc (void*)-1 nếu lỗi.
 */
void *_sbrk(ptrdiff_t incr) {
  if (!heap_end) {
    heap_end = get_heap_base();
    if (!heap_end) {
      errno = ENOMEM;
      return (void *)-1;
    }
  }

  const size_t reserve = 1024u; /* Giữ lại 1KB cho stack/ISR */
  uintptr_t estack_addr = (uintptr_t)&_estack;
  char *limit = (char *)(estack_addr - reserve);

  if (heap_end + incr > limit) {
    errno = ENOMEM;
    return (void *)-1;
  }

  char *prev = heap_end;
  heap_end += incr;
  return prev;
}

/**
 * @func    _close
 * @brief   Đóng file descriptor (stub).
 */
int _close(int fd) {
  (void)fd;
  errno = ENOSYS;
  return -1;
}

/**
 * @func    _lseek
 * @brief   Seek file (stub).
 */
off_t _lseek(int fd, off_t off, int whence) {
  (void)fd;
  (void)off;
  (void)whence;
  errno = ENOSYS;
  return -1;
}

/**
 * @func    _read
 * @brief   Đọc từ file (stub).
 */
int _read(int fd, void *buf, size_t cnt) {
  (void)fd;
  (void)buf;
  (void)cnt;
  errno = ENOSYS;
  return -1;
}

/**
 * @func    _fstat
 * @brief   Lấy thông tin file (stub).
 */
int _fstat(int fd, struct stat *st) {
  (void)fd;
  if (st) {
    st->st_mode = S_IFCHR;
  }
  return 0;
}

/**
 * @func    _isatty
 * @brief   Kiểm tra file descriptor là terminal (stub).
 */
int _isatty(int fd) {
  (void)fd;
  return 1;
}
