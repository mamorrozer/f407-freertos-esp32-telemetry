.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global g_pfnVectors
.global Reset_Handler
.extern SystemInit
.extern vPortSVCHandler
.extern xPortPendSVHandler
.extern __libc_init_array
.extern main

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word vPortSVCHandler
    .word DebugMon_Handler
    .word 0
    .word xPortPendSVHandler
    .word SysTick_Handler
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMP_STAMP_IRQHandler
    .word RTC_WKUP_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Stream0_IRQHandler
    .word DMA1_Stream1_IRQHandler
    .word DMA1_Stream2_IRQHandler
    .word DMA1_Stream3_IRQHandler
    .word DMA1_Stream4_IRQHandler
    .word DMA1_Stream5_IRQHandler
    .word DMA1_Stream6_IRQHandler
    .word ADC_IRQHandler
    .word CAN1_TX_IRQHandler
    .word CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_TIM9_IRQHandler
    .word TIM1_UP_TIM10_IRQHandler
    .word TIM1_TRG_COM_TIM11_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word OTG_FS_WKUP_IRQHandler
    .word TIM8_BRK_TIM12_IRQHandler
    .word TIM8_UP_TIM13_IRQHandler
    .word TIM8_TRG_COM_TIM14_IRQHandler
    .word TIM8_CC_IRQHandler
    .word DMA1_Stream7_IRQHandler
    .word FSMC_IRQHandler
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_DAC_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Stream0_IRQHandler
    .word DMA2_Stream1_IRQHandler
    .word DMA2_Stream2_IRQHandler
    .word DMA2_Stream3_IRQHandler
    .word DMA2_Stream4_IRQHandler
    .word ETH_IRQHandler
    .word ETH_WKUP_IRQHandler
    .word CAN2_TX_IRQHandler
    .word CAN2_RX0_IRQHandler
    .word CAN2_RX1_IRQHandler
    .word CAN2_SCE_IRQHandler
    .word OTG_FS_IRQHandler
    .word DMA2_Stream5_IRQHandler
    .word DMA2_Stream6_IRQHandler
    .word DMA2_Stream7_IRQHandler
    .word USART6_IRQHandler
    .word I2C3_EV_IRQHandler
    .word I2C3_ER_IRQHandler
    .word OTG_HS_EP1_OUT_IRQHandler
    .word OTG_HS_EP1_IN_IRQHandler
    .word OTG_HS_WKUP_IRQHandler
    .word OTG_HS_IRQHandler
    .word DCMI_IRQHandler
    .word CRYP_IRQHandler
    .word HASH_RNG_IRQHandler
    .word FPU_IRQHandler
    .word UART7_IRQHandler
    .word UART8_IRQHandler
    .word SPI4_IRQHandler
    .word SPI5_IRQHandler
    .word SPI6_IRQHandler
    .size g_pfnVectors, .-g_pfnVectors

.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_estack
    mov sp, r0
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
    b 2f
1:
    ldr r3, [r0], #4
    str r3, [r1], #4
2:
    cmp r1, r2
    bcc 1b
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
    b 2f
3:
    str r3, [r1], #4
2:
    cmp r1, r2
    bcc 3b
    bl SystemInit
    bl __libc_init_array
    bl main
4:
    b 4b
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
Default_Handler:
    b .
.size Default_Handler, .-Default_Handler

.weak NMI_Handler
.set NMI_Handler, Default_Handler
.weak HardFault_Handler
.set HardFault_Handler, Default_Handler
.weak MemManage_Handler
.set MemManage_Handler, Default_Handler
.weak BusFault_Handler
.set BusFault_Handler, Default_Handler
.weak UsageFault_Handler
.set UsageFault_Handler, Default_Handler
.weak SVC_Handler
.set SVC_Handler, Default_Handler
.weak DebugMon_Handler
.set DebugMon_Handler, Default_Handler
.weak PendSV_Handler
.set PendSV_Handler, Default_Handler
.weak SysTick_Handler
.set SysTick_Handler, Default_Handler

.weak WWDG_IRQHandler
.set WWDG_IRQHandler, Default_Handler
.weak PVD_IRQHandler
.set PVD_IRQHandler, Default_Handler
.weak TAMP_STAMP_IRQHandler
.set TAMP_STAMP_IRQHandler, Default_Handler
.weak RTC_WKUP_IRQHandler
.set RTC_WKUP_IRQHandler, Default_Handler
.weak FLASH_IRQHandler
.set FLASH_IRQHandler, Default_Handler
.weak RCC_IRQHandler
.set RCC_IRQHandler, Default_Handler
.weak EXTI0_IRQHandler
.set EXTI0_IRQHandler, Default_Handler
.weak EXTI1_IRQHandler
.set EXTI1_IRQHandler, Default_Handler
.weak EXTI2_IRQHandler
.set EXTI2_IRQHandler, Default_Handler
.weak EXTI3_IRQHandler
.set EXTI3_IRQHandler, Default_Handler
.weak EXTI4_IRQHandler
.set EXTI4_IRQHandler, Default_Handler
.weak DMA1_Stream0_IRQHandler
.set DMA1_Stream0_IRQHandler, Default_Handler
.weak DMA1_Stream1_IRQHandler
.set DMA1_Stream1_IRQHandler, Default_Handler
.weak DMA1_Stream2_IRQHandler
.set DMA1_Stream2_IRQHandler, Default_Handler
.weak DMA1_Stream3_IRQHandler
.set DMA1_Stream3_IRQHandler, Default_Handler
.weak DMA1_Stream4_IRQHandler
.set DMA1_Stream4_IRQHandler, Default_Handler
.weak DMA1_Stream5_IRQHandler
.set DMA1_Stream5_IRQHandler, Default_Handler
.weak DMA1_Stream6_IRQHandler
.set DMA1_Stream6_IRQHandler, Default_Handler
.weak ADC_IRQHandler
.set ADC_IRQHandler, Default_Handler
.weak CAN1_TX_IRQHandler
.set CAN1_TX_IRQHandler, Default_Handler
.weak CAN1_RX0_IRQHandler
.set CAN1_RX0_IRQHandler, Default_Handler
.weak CAN1_RX1_IRQHandler
.set CAN1_RX1_IRQHandler, Default_Handler
.weak CAN1_SCE_IRQHandler
.set CAN1_SCE_IRQHandler, Default_Handler
.weak EXTI9_5_IRQHandler
.set EXTI9_5_IRQHandler, Default_Handler
.weak TIM1_BRK_TIM9_IRQHandler
.set TIM1_BRK_TIM9_IRQHandler, Default_Handler
.weak TIM1_UP_TIM10_IRQHandler
.set TIM1_UP_TIM10_IRQHandler, Default_Handler
.weak TIM1_TRG_COM_TIM11_IRQHandler
.set TIM1_TRG_COM_TIM11_IRQHandler, Default_Handler
.weak TIM1_CC_IRQHandler
.set TIM1_CC_IRQHandler, Default_Handler
.weak TIM2_IRQHandler
.set TIM2_IRQHandler, Default_Handler
.weak TIM3_IRQHandler
.set TIM3_IRQHandler, Default_Handler
.weak TIM4_IRQHandler
.set TIM4_IRQHandler, Default_Handler
.weak I2C1_EV_IRQHandler
.set I2C1_EV_IRQHandler, Default_Handler
.weak I2C1_ER_IRQHandler
.set I2C1_ER_IRQHandler, Default_Handler
.weak I2C2_EV_IRQHandler
.set I2C2_EV_IRQHandler, Default_Handler
.weak I2C2_ER_IRQHandler
.set I2C2_ER_IRQHandler, Default_Handler
.weak SPI1_IRQHandler
.set SPI1_IRQHandler, Default_Handler
.weak SPI2_IRQHandler
.set SPI2_IRQHandler, Default_Handler
.weak USART1_IRQHandler
.set USART1_IRQHandler, Default_Handler
.weak USART2_IRQHandler
.set USART2_IRQHandler, Default_Handler
.weak USART3_IRQHandler
.set USART3_IRQHandler, Default_Handler
.weak EXTI15_10_IRQHandler
.set EXTI15_10_IRQHandler, Default_Handler
.weak RTC_Alarm_IRQHandler
.set RTC_Alarm_IRQHandler, Default_Handler
.weak OTG_FS_WKUP_IRQHandler
.set OTG_FS_WKUP_IRQHandler, Default_Handler
.weak TIM8_BRK_TIM12_IRQHandler
.set TIM8_BRK_TIM12_IRQHandler, Default_Handler
.weak TIM8_UP_TIM13_IRQHandler
.set TIM8_UP_TIM13_IRQHandler, Default_Handler
.weak TIM8_TRG_COM_TIM14_IRQHandler
.set TIM8_TRG_COM_TIM14_IRQHandler, Default_Handler
.weak TIM8_CC_IRQHandler
.set TIM8_CC_IRQHandler, Default_Handler
.weak DMA1_Stream7_IRQHandler
.set DMA1_Stream7_IRQHandler, Default_Handler
.weak FSMC_IRQHandler
.set FSMC_IRQHandler, Default_Handler
.weak SDIO_IRQHandler
.set SDIO_IRQHandler, Default_Handler
.weak TIM5_IRQHandler
.set TIM5_IRQHandler, Default_Handler
.weak SPI3_IRQHandler
.set SPI3_IRQHandler, Default_Handler
.weak UART4_IRQHandler
.set UART4_IRQHandler, Default_Handler
.weak UART5_IRQHandler
.set UART5_IRQHandler, Default_Handler
.weak TIM6_DAC_IRQHandler
.set TIM6_DAC_IRQHandler, Default_Handler
.weak TIM7_IRQHandler
.set TIM7_IRQHandler, Default_Handler
.weak DMA2_Stream0_IRQHandler
.set DMA2_Stream0_IRQHandler, Default_Handler
.weak DMA2_Stream1_IRQHandler
.set DMA2_Stream1_IRQHandler, Default_Handler
.weak DMA2_Stream2_IRQHandler
.set DMA2_Stream2_IRQHandler, Default_Handler
.weak DMA2_Stream3_IRQHandler
.set DMA2_Stream3_IRQHandler, Default_Handler
.weak DMA2_Stream4_IRQHandler
.set DMA2_Stream4_IRQHandler, Default_Handler
.weak ETH_IRQHandler
.set ETH_IRQHandler, Default_Handler
.weak ETH_WKUP_IRQHandler
.set ETH_WKUP_IRQHandler, Default_Handler
.weak CAN2_TX_IRQHandler
.set CAN2_TX_IRQHandler, Default_Handler
.weak CAN2_RX0_IRQHandler
.set CAN2_RX0_IRQHandler, Default_Handler
.weak CAN2_RX1_IRQHandler
.set CAN2_RX1_IRQHandler, Default_Handler
.weak CAN2_SCE_IRQHandler
.set CAN2_SCE_IRQHandler, Default_Handler
.weak OTG_FS_IRQHandler
.set OTG_FS_IRQHandler, Default_Handler
.weak DMA2_Stream5_IRQHandler
.set DMA2_Stream5_IRQHandler, Default_Handler
.weak DMA2_Stream6_IRQHandler
.set DMA2_Stream6_IRQHandler, Default_Handler
.weak DMA2_Stream7_IRQHandler
.set DMA2_Stream7_IRQHandler, Default_Handler
.weak USART6_IRQHandler
.set USART6_IRQHandler, Default_Handler
.weak I2C3_EV_IRQHandler
.set I2C3_EV_IRQHandler, Default_Handler
.weak I2C3_ER_IRQHandler
.set I2C3_ER_IRQHandler, Default_Handler
.weak OTG_HS_EP1_OUT_IRQHandler
.set OTG_HS_EP1_OUT_IRQHandler, Default_Handler
.weak OTG_HS_EP1_IN_IRQHandler
.set OTG_HS_EP1_IN_IRQHandler, Default_Handler
.weak OTG_HS_WKUP_IRQHandler
.set OTG_HS_WKUP_IRQHandler, Default_Handler
.weak OTG_HS_IRQHandler
.set OTG_HS_IRQHandler, Default_Handler
.weak DCMI_IRQHandler
.set DCMI_IRQHandler, Default_Handler
.weak CRYP_IRQHandler
.set CRYP_IRQHandler, Default_Handler
.weak HASH_RNG_IRQHandler
.set HASH_RNG_IRQHandler, Default_Handler
.weak FPU_IRQHandler
.set FPU_IRQHandler, Default_Handler
.weak UART7_IRQHandler
.set UART7_IRQHandler, Default_Handler
.weak UART8_IRQHandler
.set UART8_IRQHandler, Default_Handler
.weak SPI4_IRQHandler
.set SPI4_IRQHandler, Default_Handler
.weak SPI5_IRQHandler
.set SPI5_IRQHandler, Default_Handler
.weak SPI6_IRQHandler
.set SPI6_IRQHandler, Default_Handler
