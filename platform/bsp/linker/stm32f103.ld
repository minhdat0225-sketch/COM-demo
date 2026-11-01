/*======== stm32f103.ld ============
  Linker script cho STM32F103 (64 KB Flash, 20 KB RAM)
  Định nghĩa _sidata, _sdata, _edata, _sbss, _ebss
====================================*/

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K  /* 0x08000000–0x08010000 */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K  /* 0x20000000–0x20005000 */
}

/* Entry point khi MCU reset */
ENTRY(Reset_Handler)

/* Đỉnh stack ở cuối RAM */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* ==== Bảng vector ngắt ==== */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    /* ==== Mã chương trình & hằng số ==== */
    .text :
    {
        _stext = .;
        *(.text*)                 /* code */
        *(.rodata*)               /* const */
        *(.glue_7) *(.glue_7t)
    } > FLASH

    /* ==== Bảng unwind của ARM (nếu có) đặt TRONG FLASH, TRƯỚC _etext ==== */
    . = ALIGN(4);
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH

    . = ALIGN(4);
    __exidx_start = .;
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH
    __exidx_end = .;

    /* ==== Đánh dấu cuối vùng ROM dùng để copy .data ==== */
    . = ALIGN(4);
    _etext = .;

    /* ==== .data ở RAM, copy từ FLASH tại địa chỉ _etext ==== */
    .data : AT(_etext)
    {
        . = ALIGN(4);
        _sdata  = .;                      /* RAM begin of .data */
        *(.data*) *(.data.*)
        . = ALIGN(4);
        _edata  = .;                      /* RAM end of .data */
    } > RAM
    /* Nguồn copy cho startup (địa chỉ trong FLASH) */
    _sidata = LOADADDR(.data);

    /* ==== .bss ở RAM, không nạp ==== */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*) *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* ==== Loại bỏ các section không cần ==== */
    /DISCARD/ :
    {
        *(.note*)
        *(.comment*)
    }
}
