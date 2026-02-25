/*
 * Linker script for OSPI RTT Flash Programmer
 * Target: Alif E7 M55_HP core
 *
 * ITCM @ 0x00000000 (local) = 0x50000000 (global/ATOC loadAddress)
 * DTCM @ 0x20000000 (local)
 */

MEMORY
{
    ITCM (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    DTCM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* Code and read-only data in ITCM */
    .text : {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > ITCM

    /* Initialized data: load from ITCM, copy to DTCM at startup */
    .data : AT(LOADADDR(.text) + SIZEOF(.text)) {
        _data_start = .;
        *(.data*)
        . = ALIGN(4);
        _data_end = .;
    } > DTCM

    _data_load = LOADADDR(.data);

    /* Zero-initialized data in DTCM */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > DTCM

    /* Stack at top of DTCM */
    _stack_top = ORIGIN(DTCM) + LENGTH(DTCM);
}
