.syntax unified
.cpu arm7tdmi
.arm
.global _start
_start:
    mov r0, #3
    mov r1, #4
    mul r3, r0, r1
    mla r5, r0, r1, r3
    stmia r2!, {r0, r1, r3, r5}
    sub r2, r2, #16
    ldmia r2, {r6-r9}
    cmp r9, #24
    bne wrong
    mov r10, #1
    b done
wrong:
    mov r10, #0
done:
    b done
