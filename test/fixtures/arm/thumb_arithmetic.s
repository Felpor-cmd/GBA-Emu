.syntax unified
.cpu arm7tdmi
.arm
.global _start
_start:
    ldr r0, =thumb_start + 1
    bx r0
.thumb
thumb_start:
    movs r0, #3
    movs r1, #4
    adds r0, r0, r1
    cmp r0, #7
    beq equal
    movs r3, #99
equal:
    lsls r0, r0, #1
    bl subroutine
    b done
subroutine:
    adds r0, #1
    bx lr
done:
    str r0, [r2, #0]
    ldr r4, [r2, #0]
    b done
.align 2
.ltorg
