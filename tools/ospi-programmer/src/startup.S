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
    /*
     * Security configuration for Alif E7 M55_HP.
     *
     * Constraints discovered through testing:
     * - ALLNS=1 (SAU_CTRL bit 1) kills JLink debug completely
     * - NSC=0 covering all memory also kills debug (same effect)
     * - DSB/ISB stall after ITGU/DTGU/SAU register writes
     * - DTGU (0xE001F000) writes hang the CPU
     * - OSPI controller (0x83002000) BusFaults from CPU regardless of
     *   SAU config (tested: no SAU, ALLNS, split NS regions)
     * - Debug DAP reads OSPI registers fine — peripheral is accessible
     *   on AXI bus, but M55_HP CPU goes through EXPMST bridge which
     *   may not forward 0x8xxx_xxxx addresses without additional config
     *
     * Current: SAU disabled (all Secure). OSPI access needs EXPMST
     * bus master bridge investigation — not a TrustZone issue.
     */

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
