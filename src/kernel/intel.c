#include <stdbool.h>

#include <kernel/intel.h>
#include <kernel/terminal.h>

#include <klegit/string.h>

#define ICW1_ICW4   0x01        /* ICW4 (not) needed */
#define ICW1_SINGLE 0x02        /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04        /* Call address interval 4 (8) */
#define ICW1_LEVEL  0x08        /* Level triggered (edge) mode */
#define ICW1_INIT   0x10        /* Initialization - required! */
 
#define ICW4_8086   0x01        /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO   0x02        /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08        /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C        /* Buffered mode/master */
#define ICW4_SFNM   0x10        /* Special fully nested (not) */

#define PIC1        0x20        /* IO base address for master PIC */
#define PIC2        0xA0        /* IO base address for slave PIC */
#define PIC1_COMMAND    PIC1
#define PIC1_DATA   (PIC1+1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA   (PIC2+1)

#define GDT_ENTRY_COUNT 6
#define IDT_ENTRY_COUNT 256

#define START_INTERRUPT_LABEL(interrupt_number)\
__asm__ goto ( \
    "mov $%l1, %%eax\n\t" \
    "mov %%eax, (%%ebx)" \
    : \
    : "b"(&handlers[interrupt_number]) \
    : "memory", "eax" \
    : start_of_interrupt_handler##interrupt_number \
); \
__asm__ goto ( \
    "jmp $0x8, $%l0" \
    : \
    : \
    : \
    : end_of_interrupt_handler##interrupt_number \
); \
start_of_interrupt_handler##interrupt_number:

#define END_INTERRUPT_LABEL(interrupt_number)\
end_of_interrupt_handler##interrupt_number:

struct gdt_entry_struct gdt_entries[GDT_ENTRY_COUNT];
struct idt_entry_struct idt_entries[IDT_ENTRY_COUNT];
struct tss_struct tss;
struct gdt_pointer_struct gdt;
struct idt_pointer_struct idt;

/* bochs magic breakpoint */
void bochs_break() {
     __asm__ volatile ("xchgw %bx, %bx");
}

/* write a byte to an IO port */
void outb(unsigned int port, unsigned char byte) {
    __asm__ volatile ("outb %%al, %%dx" : : "d" (port), "a" (byte));
}

/* read a byte from an IO port */
unsigned char inb(unsigned int port) {
    unsigned char byte;
    __asm__ volatile ("inb %%dx, %%al" : "=a"(byte) : "d" (port));
    return byte;
}

/* halt the CPU in a way that hopefully doesn't cause it to catch fire */
void halt() {
    terminal_write("Halting CPU.");

    __asm__ volatile ("cli");

    /* call hlt forever. the first call should halt the CPU indefinitely as interrupts are off, but just in case... */
    while (true) {
        __asm__ volatile ("hlt");
    }
}

/* build and return a GDT entry */
struct gdt_entry_struct gdt_entry(uint32_t base_address, uint32_t limit_address, uint8_t access_flags, uint8_t granularity_flags) {
    struct gdt_entry_struct entry;

    entry.base_address_low_word = base_address & 0xFFFF;
    entry.base_address_middle_byte = (base_address >> 16) & 0xFF;
    entry.base_address_high_byte   = (base_address >> 24) & 0xFF;

    entry.limit_address_low_word = (limit_address & 0xFFFF);
    entry.granularity_flags_and_limit_address_high_nibble = (limit_address >> 16) & 0x0F;
    entry.granularity_flags_and_limit_address_high_nibble |= (granularity_flags & 0xF0);

    entry.access_flags = access_flags;

    return entry;
}

/* load the gdtr register with the location of the gdt_pointer, then set the segmentation registers to match the new GDT layout */
void switch_to_gdt(struct gdt_pointer_struct* gdt) {
    /*
    0x08 = the first entry in the GDT
    0x10 = the second entry

    and so on
    */

    __asm__ goto (
        "lgdt (%0)\n\t"
        "jmp $0x08, $%l1" /* this jump sets cs implicitly */
        : 
        : "r"(gdt) 
        : 
        : change_data_segments
    );
    change_data_segments:
    __asm__ volatile (
        "mov $0x10, %%ax\n\t" /* now set the data segment in the other registers */
        "mov %%ax, %%ds\n\t" /* n.b. you cannot set the segment registers to an immediate value... */
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss"
        :
        :
        : "%ax"
    );
}

/* load the idtr register with the location of the idt_pointer */
void switch_to_idt(struct idt_pointer_struct* idt) {
    __asm__ volatile (
        "lidt (%0)"
        : 
        : "r"(idt) 
        : 
    );
}

/* build and return an IDT entry */
struct idt_entry_struct idt_entry(uint32_t offset, uint16_t code_selector, uint8_t type_and_attributes) {
    struct idt_entry_struct entry;

    entry.offset_low_word = offset & 0xFFFF;
    entry.code_selector = code_selector;   
    entry.zero = 0;
    entry.type_and_attributes = type_and_attributes | 0x80;   
    entry.offset_high_word = (offset >> 16) & 0xFFFF;

    return entry;
}

void setup_pic() {
    unsigned char first_pic_mask;
    unsigned char second_pic_mask;
 
    first_pic_mask = inb(PIC1_DATA)
    second_pic_mask = inb(PIC2_DATA);
 
    outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
    outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);

    outb(PIC1_DATA, 0x20); /* set offset for first PIC */
    outb(PIC2_DATA, 0x28); /* set offset for 2nd PIC */

    outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
 
    outb(PIC1_DATA, a1);   // restore saved masks.
    outb(PIC2_DATA, a2);
}

/* initialise the IDT and interrupts. this is also where the interrupt handler shims are defined. */
void setup_idt() {
    uint32_t handlers[IDT_ENTRY_COUNT];

    idt.limit = sizeof(struct idt_entry_struct) * IDT_ENTRY_COUNT - 1;
    idt.base = (uint32_t)&idt_entries;
    memset(&idt_entries, 0, sizeof(idt_entries));

    START_INTERRUPT_LABEL(0);
        terminal_write("Division by 0.\n");
        bochs_break();
    END_INTERRUPT_LABEL(0);

    START_INTERRUPT_LABEL(1);
        terminal_write("Debug.\n");
        bochs_break();
    END_INTERRUPT_LABEL(1);

    START_INTERRUPT_LABEL(2);
        terminal_write("NMI.\n");
        bochs_break();
    END_INTERRUPT_LABEL(2);

    START_INTERRUPT_LABEL(3);
        terminal_write("Breakpoint.\n");
        bochs_break();
    END_INTERRUPT_LABEL(3);

    START_INTERRUPT_LABEL(4);
        terminal_write("INTO.\n");
        bochs_break();
    END_INTERRUPT_LABEL(4);

    START_INTERRUPT_LABEL(5);
        terminal_write("BOUND.\n");
        bochs_break();
    END_INTERRUPT_LABEL(5);    

    START_INTERRUPT_LABEL(6);
        terminal_write("Invalid opcode.\n");
        bochs_break();
    END_INTERRUPT_LABEL(6);

    START_INTERRUPT_LABEL(7);
        terminal_write("No co-processr.\n");
        bochs_break();
    END_INTERRUPT_LABEL(7);    

    START_INTERRUPT_LABEL(8);
        terminal_write("Double fault.\n");
        bochs_break();
    END_INTERRUPT_LABEL(8);      

    START_INTERRUPT_LABEL(9);
        terminal_write("Co-processor segment overrun.\n");
        bochs_break();
    END_INTERRUPT_LABEL(9);    

    START_INTERRUPT_LABEL(10);
        terminal_write("TSS problem.\n");
        bochs_break();
    END_INTERRUPT_LABEL(10);   

    START_INTERRUPT_LABEL(11);
        terminal_write("Segment missing.\n");
        bochs_break();
    END_INTERRUPT_LABEL(11);       

    START_INTERRUPT_LABEL(12);
        terminal_write("Stack fault.\n");
        bochs_break();
    END_INTERRUPT_LABEL(12);       

    START_INTERRUPT_LABEL(13);
        terminal_write("General protection fault.\n");
        bochs_break();
    END_INTERRUPT_LABEL(13); 

    START_INTERRUPT_LABEL(14);
        terminal_write("Page fault.\n");
        bochs_break();
    END_INTERRUPT_LABEL(14); 

    START_INTERRUPT_LABEL(16);
        terminal_write("Co-processor problem.\n");
        bochs_break();
    END_INTERRUPT_LABEL(16);

    START_INTERRUPT_LABEL(32);
        terminal_write("IRQ 0.\n");
        bochs_break();
    END_INTERRUPT_LABEL(32);

    START_INTERRUPT_LABEL(33);
        terminal_write("IRQ 1.\n");
        bochs_break();
    END_INTERRUPT_LABEL(33);

    START_INTERRUPT_LABEL(34);
        terminal_write("IRQ 2.\n");
        bochs_break();
    END_INTERRUPT_LABEL(34);

    START_INTERRUPT_LABEL(35);
        terminal_write("IRQ 3.\n");
        bochs_break();
    END_INTERRUPT_LABEL(35);

    START_INTERRUPT_LABEL(36);
        terminal_write("IRQ 4.\n");
        bochs_break();
    END_INTERRUPT_LABEL(36);

    START_INTERRUPT_LABEL(37);
        terminal_write("IRQ 5.\n");
        bochs_break();
    END_INTERRUPT_LABEL(37);    

    START_INTERRUPT_LABEL(38);
        terminal_write("IRQ 6.\n");
        bochs_break();
    END_INTERRUPT_LABEL(38);

    START_INTERRUPT_LABEL(39);
        terminal_write("IRQ 7.\n");
        bochs_break();
    END_INTERRUPT_LABEL(39);    

    START_INTERRUPT_LABEL(40);
        terminal_write("IRQ 8.\n");
        bochs_break();
    END_INTERRUPT_LABEL(40);      

    START_INTERRUPT_LABEL(41);
        terminal_write("IRQ 9.\n");
        bochs_break();
    END_INTERRUPT_LABEL(41);    

    START_INTERRUPT_LABEL(42);
        terminal_write("IRQ 10.\n");
        bochs_break();
    END_INTERRUPT_LABEL(42);   

    START_INTERRUPT_LABEL(43);
        terminal_write("IRQ 11.\n");
        bochs_break();
    END_INTERRUPT_LABEL(43);       

    START_INTERRUPT_LABEL(44);
        terminal_write("IRQ 12.\n");
        bochs_break();
    END_INTERRUPT_LABEL(44);       

    START_INTERRUPT_LABEL(45);
        terminal_write("IRQ 13.\n");
        bochs_break();
    END_INTERRUPT_LABEL(45); 

    START_INTERRUPT_LABEL(46);
        terminal_write("IRQ 14.\n");
        bochs_break();
    END_INTERRUPT_LABEL(46); 

    START_INTERRUPT_LABEL(47);
        terminal_write("IRQ 15.\n");
        bochs_break();
    END_INTERRUPT_LABEL(47);

    START_INTERRUPT_LABEL(128);
        terminal_write("Syscall.\n");
        bochs_break();
    END_INTERRUPT_LABEL(128);    

    /* software interrupts */
    idt_entries[0] = idt_entry(handlers[0], 0x8, 0xe);
    idt_entries[1] = idt_entry(handlers[1], 0x8, 0xe);
    idt_entries[2] = idt_entry(handlers[2], 0x8, 0xe);
    idt_entries[3] = idt_entry(handlers[3], 0x8, 0xe);
    idt_entries[4] = idt_entry(handlers[4], 0x8, 0xe);
    idt_entries[5] = idt_entry(handlers[5], 0x8, 0xe);
    idt_entries[6] = idt_entry(handlers[6], 0x8, 0xe);
    idt_entries[7] = idt_entry(handlers[7], 0x8, 0xe);
    idt_entries[8] = idt_entry(handlers[8], 0x8, 0xe);
    idt_entries[9] = idt_entry(handlers[9], 0x8, 0xe);
    idt_entries[10] = idt_entry(handlers[10], 0x8, 0xe);
    idt_entries[11] = idt_entry(handlers[11], 0x8, 0xe);
    idt_entries[12] = idt_entry(handlers[12], 0x8, 0xe);
    idt_entries[13] = idt_entry(handlers[13], 0x8, 0xe);
    idt_entries[14] = idt_entry(handlers[14], 0x8, 0xe);
    idt_entries[16] = idt_entry(handlers[16], 0x8, 0xe);
    idt_entries[32] = idt_entry(handlers[32], 0x8, 0xe);
    idt_entries[33] = idt_entry(handlers[33], 0x8, 0xe);
    idt_entries[34] = idt_entry(handlers[34], 0x8, 0xe);
    idt_entries[35] = idt_entry(handlers[35], 0x8, 0xe);
    idt_entries[36] = idt_entry(handlers[36], 0x8, 0xe);
    idt_entries[37] = idt_entry(handlers[37], 0x8, 0xe);
    idt_entries[38] = idt_entry(handlers[38], 0x8, 0xe);
    idt_entries[39] = idt_entry(handlers[39], 0x8, 0xe);
    idt_entries[40] = idt_entry(handlers[40], 0x8, 0xe);
    idt_entries[41] = idt_entry(handlers[41], 0x8, 0xe);
    idt_entries[42] = idt_entry(handlers[42], 0x8, 0xe);
    idt_entries[43] = idt_entry(handlers[43], 0x8, 0xe);
    idt_entries[44] = idt_entry(handlers[44], 0x8, 0xe);
    idt_entries[45] = idt_entry(handlers[45], 0x8, 0xe);
    idt_entries[46] = idt_entry(handlers[46], 0x8, 0xe);
    idt_entries[47] = idt_entry(handlers[47], 0x8, 0xe);    
    idt_entries[0x80] = idt_entry(handlers[128], 0x8, 0xe);

    terminal_write("This IDT was built (first 8 entries):\n");
    terminal_hexdump(idt_entries, sizeof(struct idt_entry_struct) * 8);

    switch_to_idt(&idt);

    terminal_write("Calling test interrupt...\n");


    PIC_remap(0x20, 0x28);


    /* __asm__ volatile ("sti");
    while (1) {

    } */

    __asm__ volatile ("int $0x80"); 

}

/* initialise the GDT. */
void setup_gdt() {
    gdt.limit = sizeof(struct gdt_entry_struct) * GDT_ENTRY_COUNT - 1;
    gdt.base = (uint32_t)&gdt_entries;

    /* construct a basic GDT which maps all of memory */
    gdt_entries[0] = gdt_entry(0, 0, 0, 0);
    gdt_entries[1] = gdt_entry(0, 0xFFFFFFFF, 0x9A, 0xC0); /* ring 0 code segment */
    gdt_entries[2] = gdt_entry(0, 0xFFFFFFFF, 0x92, 0xC0); /* ring 0 data segment */
    gdt_entries[3] = gdt_entry(0, 0xFFFFFFFF, 0xFA, 0xC0); /* ring 3 code segment */
    gdt_entries[4] = gdt_entry(0, 0xFFFFFFFF, 0xF2, 0xC0); /* ring 3 code segment */
    gdt_entries[5] = gdt_entry((uint32_t)&tss, sizeof(tss), 0x89, 0xC0); /* TSS for later multitasking */

    switch_to_gdt(&gdt);
}
