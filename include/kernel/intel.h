#ifndef KERNEL_CPU_HEADER
#define KERNEL_CPU_HEADER

#include <stdint.h>

/*

GDT access flags byte
---------------------

+-----+-----+-----+-----+-----+-----+-----+-----+
|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
+-----+-----+-----+-----+-----+-----+-----+-----+
|  P  |    DPL    | DT  |         Type          |
+-----+-----+-----+-----+-----+-----+-----+-----+

P = Segment is "present"
DPL = Ring level
DT = Descriptor type
Type = Segment type

GDT granularity flags byte
--------------------------

+-----+-----+-----+-----+-----+-----+-----+-----+
|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
+-----+-----+-----+-----+-----+-----+-----+-----+
|  G  |  D  |  0  |  A  |     Segment length    |
+-----+-----+-----+-----+-----+-----+-----+-----+

G = Granularity (0 for '1 byte', 1 for '1 kilobyte')
D = Operand size (0 for '16 bit', 1 for '32 bit')
0 = Always 0
A = Reserved

IDT type and attributes byte
----------------------------

+-----+-----+-----+-----+-----+-----+-----+-----+
|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
+-----+-----+-----+-----+-----+-----+-----+-----+
|  P  |    DPL    |  S  |         Type          |
+-----+-----+-----+-----+-----+-----+-----+-----+

P = "present" for paging
DPL = Ring level required to run this interrupt
S = 0 for an interrupt gate
Type = If S is 0, the interrupt gate type:

0x5 80386 32 bit task gate
0x6 80286 16-bit interrupt gate
0x7 80286 16-bit trap gate
0xE 80386 32-bit interrupt gate
0xF 80386 32-bit trap gate

*/

struct __attribute__((__packed__)) gdt_entry_struct {
    uint16_t limit_address_low_word;
    uint16_t base_address_low_word;
    uint8_t base_address_middle_byte;
    uint8_t access_flags;
    uint8_t granularity_flags_and_limit_address_high_nibble;
    uint8_t base_address_high_byte;
};

struct __attribute__((__packed__)) gdt_pointer_struct {
    uint16_t limit;
    uint32_t base;
};

struct __attribute__((__packed__)) idt_entry_struct {
    uint16_t offset_low_word;
    uint16_t code_selector;
    uint8_t zero;     
    uint8_t type_and_attributes;
    uint16_t offset_high_word;
};

struct __attribute__((__packed__)) idt_pointer_struct {
    uint16_t limit;
    uint32_t base;
};

struct __attribute__((__packed__)) tss_struct {
    uint16_t link;
    uint16_t link_h;
    uint32_t esp0;
    uint16_t ss0;
    uint16_t ss0_h;
    uint32_t esp1;
    uint16_t ss1;
    uint16_t ss1_h;
    uint32_t esp2;
    uint16_t ss2;
    uint16_t ss2_h;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es;
    uint16_t es_h;
    uint16_t cs;
    uint16_t cs_h;
    uint16_t ss;
    uint16_t ss_h;
    uint16_t ds;
    uint16_t ds_h;
    uint16_t fs;
    uint16_t fs_h;
    uint16_t gs;
    uint16_t gs_h;
    uint16_t ldt;
    uint16_t ldt_h;
    uint16_t trap;
    uint16_t iomap;
};

void outb(unsigned int port, unsigned char byte);
void halt();
void setup_gdt();
void setup_idt();

#endif
