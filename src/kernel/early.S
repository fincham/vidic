# multiboot header
.section .multiboot
.align 4
.long 0x1BADB002 # multiboot magic
.long 3 # flags for "align modules on page boundaries" and "provide memory map info"
.long -464367621 # checksum of the above (inverse of flags + magic)

.section .text
.global _start
.type _start, @function
_start:
    movl $early_stack_top, %esp # set up the mini stack
    call _init # call the global constructors
    call main # call the kernel's main function

# set up a 32k stack for early boot
.section .early_stack, "aw", @nobits
early_stack_bottom:
.skip 32768
early_stack_top:
