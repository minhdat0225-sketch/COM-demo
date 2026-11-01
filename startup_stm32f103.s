/* startup_stm32f103.s – COM demo
 * Startup file for STM32F103 with CAN/LIN ISR vector stubs
 */

    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .isr_vector,"a",%progbits
    .type g_pfnVectors, %object

g_pfnVectors:
    .word _estack               /* Initial stack pointer */
    .word Reset_Handler         /* 1:  Reset */
    .word Default_Handler       /* 2:  NMI */
    .word Default_Handler       /* 3:  HardFault */
    .word Default_Handler       /* 4:  MemManage */
    .word Default_Handler       /* 5:  BusFault */
    .word Default_Handler       /* 6:  UsageFault */
    .word 0                     /* 7:  Reserved */
    .word 0                     /* 8:  Reserved */
    .word 0                     /* 9:  Reserved */
    .word 0                     /* 10: Reserved */
    .word Default_Handler       /* 11: SVCall */
    .word Default_Handler       /* 12: DebugMonitor */
    .word 0                     /* 13: Reserved */
    .word Default_Handler       /* 14: PendSV */
    .word Default_Handler       /* 15: SysTick */
    /* External Interrupts */
    .word Default_Handler       /* 16: WWDG */
    .word Default_Handler       /* 17: PVD */
    .word Default_Handler       /* 18: TAMPER */
    .word Default_Handler       /* 19: RTC */
    .word Default_Handler       /* 20: FLASH */
    .word Default_Handler       /* 21: RCC */
    .word EXTI0_IRQHandler       /* 22: EXTI0 */
    .word Default_Handler       /* 23: EXTI1 */
    .word Default_Handler       /* 24: EXTI2 */
    .word Default_Handler       /* 25: EXTI3 */
    .word Default_Handler       /* 26: EXTI4 */
    .word DMA1_Channel1_IRQHandler       /* 27: DMA1_Channel1 */
    .word Default_Handler       /* 28: DMA1_Channel2 */
    .word Default_Handler       /* 29: DMA1_Channel3 */
    .word Default_Handler       /* 30: DMA1_Channel4 */
    .word Default_Handler       /* 31: DMA1_Channel5 */
    .word Default_Handler       /* 32: DMA1_Channel6 */
    .word Default_Handler       /* 33: DMA1_Channel7 */
    .word ADC1_2_IRQHandler       /* 34: ADC1_2 */
    .word USB_HP_CAN1_TX_IRQHandler       /* 35: USB_HP_CAN1_TX (CAN TX) */
    .word USB_LP_CAN1_RX0_IRQHandler  /* 36: USB_LP_CAN1_RX0 */
    .word Default_Handler       /* 37: CAN1_RX1 */
    .word Default_Handler       /* 38: CAN1_SCE */
    .word Default_Handler       /* 39: EXTI9_5 */
    .word Default_Handler       /* 40: TIM1_BRK */
    .word Default_Handler       /* 41: TIM1_UP */
    .word Default_Handler       /* 42: TIM1_TRG_COM */
    .word Default_Handler       /* 43: TIM1_CC */
    .word Default_Handler       /* 44: TIM2 */
    .word TIM3_IRQHandler       /* 45: TIM3 */
    .word Default_Handler       /* 46: TIM4 */
    .word I2C1_EV_IRQHandler       /* 47: I2C1_EV */
    .word Default_Handler       /* 48: I2C1_ER */
    .word Default_Handler       /* 49: I2C2_EV */
    .word Default_Handler       /* 50: I2C2_ER */
    .word SPI1_IRQHandler       /* 51: SPI1 */
    .word Default_Handler       /* 52: SPI2 */
    .word USART1_IRQHandler       /* 53: USART1 */
    .word USART2_IRQHandler       /* 54: USART2 */
    .word Default_Handler       /* 55: USART3 */

    .size g_pfnVectors, .-g_pfnVectors

    /* Default handler: infinite loop */
    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b Default_Handler
    .size Default_Handler, .-Default_Handler

    /* Reset Handler */
    .section .text.Reset_Handler,"ax",%progbits
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Copy .data from Flash to RAM */
    ldr r0, =_data
    ldr r1, =_edata
    ldr r2, =_etext
copy_data:
    cmp r0, r1
    bge clear_bss
    ldr r3, [r2], #4
    str r3, [r0], #4
    b copy_data

    /* Clear .bss */
clear_bss:
    ldr r0, =_bss
    ldr r1, =_ebss
    mov r2, #0
bss_loop:
    cmp r0, r1
    bge start_main
    str r2, [r0], #4
    b bss_loop

start_main:
    bl main
    b .

    .size Reset_Handler, .-Reset_Handler

    .weak ADC1_2_IRQHandler
    .thumb_set ADC1_2_IRQHandler,Default_Handler

    .weak DMA1_Channel1_IRQHandler
    .thumb_set DMA1_Channel1_IRQHandler,Default_Handler

    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler,Default_Handler

    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler,Default_Handler

    .weak USB_HP_CAN1_TX_IRQHandler
    .thumb_set USB_HP_CAN1_TX_IRQHandler,Default_Handler

    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler,Default_Handler

    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler,Default_Handler

    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler,Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler,Default_Handler

