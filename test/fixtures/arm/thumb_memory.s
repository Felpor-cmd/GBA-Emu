.syntax unified
.cpu arm7tdmi
.arm
.global _start
_start:
    ldr r0, =thumb_start + 1
    bx r0
.thumb
thumb_start:
    movs r0, #128
    movs r1, #1
    strb r0, [r2, #1]
    ldrsb r3, [r2, r1]
    strh r0, [r2, #4]
    ldrh r4, [r2, #4]
    str r3, [r2, #8]
    ldr r5, [r2, #8]
    push {r0, r1, lr}
    pop {r6, r7}
    pop {r0}
    add r6, pc, #0
    add r7, sp, #4
    sub sp, #4
    add sp, #4
    stmia r2!, {r0, r1}
    ldmia r2!, {r0, r1}
    stmia r2!, {r2, r3}
    .hword 0xC200          @ ARMv4 STMIA r2!,{} stores PC and adds 0x40
done:
    b done
.align 2
.ltorg
