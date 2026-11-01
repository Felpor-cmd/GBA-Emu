.syntax unified
.cpu arm7tdmi
.arm
.global _start
_start:
    mov r0, #5
    add r1, r0, #3
    str r1, [r2]
    ldr r3, [r2]
    cmp r3, #8
    bne wrong
    b done
wrong:
    mov r4, #99
done:
    mov r4, #1
    b done
