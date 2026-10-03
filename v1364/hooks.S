.section .text
/* Tail trampolines leave no extra frame while original C++ code runs, so the
 * original exception/unwind tables remain applicable to its original caller. */
.global scan_entry
.type scan_entry,@function
scan_entry:
    sub $0x28, %rsp
    mov %rdi, 0(%rsp)
    mov %rsi, 8(%rsp)
    mov %rdx, 16(%rsp)
    mov %rcx, 24(%rsp)
    call scan_begin
    mov 0(%rsp), %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    mov 24(%rsp), %rcx
    add $0x28, %rsp
    jmp original_orphan_scan
.size scan_entry, .-scan_entry

.global native_entry
.type native_entry,@function
native_entry:
    sub $0x38, %rsp
    mov %rdi, 0(%rsp)
    mov %rsi, 8(%rsp)
    mov %rdx, 16(%rsp)
    mov %rcx, 24(%rsp)
    mov %r8, 32(%rsp)
    mov %r9, 40(%rsp)
    call native_begin
    mov 0(%rsp), %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    mov 24(%rsp), %rcx
    mov 32(%rsp), %r8
    mov 40(%rsp), %r9
    add $0x38, %rsp
    jmp original_native_unlock
.size native_entry, .-native_entry

/* The fourth root occupies verified unused padding at rbp-0x38. All three
 * original roots, the frame size, and exception landing pads are preserved. */
.global fill_fourth_root
.type fill_fourth_root,@function
fill_fourth_root:
    lea external_root(%rip), %rax
    mov %rax, -0x38(%rbp)
    mov original_root1(%rip), %rax
    jmp original_root_resume
.size fill_fourth_root, .-fill_fourth_root

.macro indirect_wrapper name, impl, result_slot
.global \name
.type \name,@function
\name:
    sub $8, %rsp
    mov %rax, %r8
    call \impl
    add $8, %rsp
    /* Replay the displaced original result store in the caller's frame. */
    mov %eax, \result_slot(%rbp)
    ret
.size \name, .-\name
.endm
indirect_wrapper mount_first, mount_logged, -0x688
indirect_wrapper mount_second, mount_logged, -0x69c
indirect_wrapper unmount_entry, unmount_logged, -0x6e0
.section .note.GNU-stack,"",@progbits
