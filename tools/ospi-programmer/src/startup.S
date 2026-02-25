/*
 * Minimal Cortex-M55 startup for OSPI RTT Flash Programmer
 *
 * Vector table: SP (top of DTCM) + Reset_Handler.
 * Copies .data from ITCM to DTCM, zeros .bss, calls main().
 */

    .syntax unified
    .cpu cortex-m33
    .thumb

    .section .vectors, "a"
    .align 2
    .globl __vectors
__vectors:
    .word _stack_top        /* Initial SP */
    .word Reset_Handler     /* Reset */
    .word Default_Handler   /* NMI */
    .word Default_Handler   /* HardFault */
    .word Default_Handler   /* MemManage */
    .word Default_Handler   /* BusFault */
    .word Default_Handler   /* UsageFault */
    .word Default_Handler   /* SecureFault */
    .word 0                 /* Reserved */
    .word 0                 /* Reserved */
    .word 0                 /* Reserved */
    .word Default_Handler   /* SVCall */
    .word Default_Handler   /* DebugMon */
    .word 0                 /* Reserved */
    .word Default_Handler   /* PendSV */
    .word Default_Handler   /* SysTick */

    .text
    .thumb_func
    .globl Reset_Handler
Reset_Handler:
    /* Copy .data from ITCM (load address) to DTCM (run address) */
    ldr r0, =_data_start
    ldr r1, =_data_end
    ldr r2, =_data_load
.Lcopy_data:
    cmp r0, r1
    bge .Lzero_bss
    ldr r3, [r2], #4
    str r3, [r0], #4
    b .Lcopy_data

    /* Zero .bss */
.Lzero_bss:
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    movs r2, #0
.Lzero_loop:
    cmp r0, r1
    bge .Lcall_main
    str r2, [r0], #4
    b .Lzero_loop

.Lcall_main:
    bl main
    /* If main returns, loop forever */
.Lhalt:
    wfi
    b .Lhalt

    .thumb_func
    .globl Default_Handler
Default_Handler:
    bkpt #0
    b Default_Handler
