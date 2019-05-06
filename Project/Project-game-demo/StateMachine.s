.local currentState

.text

cases: .word case0
       .word case1
       .word default

.global 
advanceState:
cmp #2, &currentState
jnc default
mov &currentState, r4
add r4, r4
mov cases(r4), r0
case0: 
    mov #2500, r12
    call #buzzerSetPeriod
    add #1, &currentState
    jmp end 
case1: 
    mov #5000, r12
    call #buzzerSetPeriod
    add #1. &currentState
    jmp end
default:
    mov #1000, r12
    call #buzzerSetPeriod
    mov #0, &currentState
    jmp end
end:
    ret
