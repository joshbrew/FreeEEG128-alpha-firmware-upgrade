.syntax unified
.cpu cortex-m7
.thumb
.section .text.settings_probe_word,"ax",%progbits
.global settings_probe_word
.type settings_probe_word,%function
.thumb_func
settings_probe_word:
.global settings_probe_load
settings_probe_load:
    ldr r1, [r0]
    movs r0, #1
    bx lr
.global settings_probe_failed
settings_probe_failed:
    movs r0, #0
    bx lr
.size settings_probe_word, .-settings_probe_word

.section .text.BusFault_Handler,"ax",%progbits
.global BusFault_Handler
.type BusFault_Handler,%function
.thumb_func
BusFault_Handler:
    tst lr, #4
    ite eq
    mrseq r0, msp
    mrsne r0, psp
    tst lr, #16
    it eq
    addeq r0, r0, #72
    b settings_bus_fault
.size BusFault_Handler, .-BusFault_Handler
