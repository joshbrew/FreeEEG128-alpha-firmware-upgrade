.syntax unified
.cpu cortex-m7
.thumb
.section .text.uf2_probe_word,"ax",%progbits
.global uf2_probe_word
.type uf2_probe_word,%function
.thumb_func
uf2_probe_word:
.global uf2_probe_load
uf2_probe_load:
    ldr r1, [r0]
    movs r0, #1
    bx lr
.global uf2_probe_failed
uf2_probe_failed:
    movs r0, #0
    bx lr
.size uf2_probe_word, .-uf2_probe_word

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
    b uf2_bus_fault
.size BusFault_Handler, .-BusFault_Handler
