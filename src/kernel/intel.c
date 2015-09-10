#include <stdbool.h>

#include <kernel/intel.h>
#include <kernel/terminal.h>

#include <klegit/string.h>

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

/* halt the CPU in a way that hopefully doesn't cause it to catch fire */
void halt() {
    terminal_write("Halting CPU.");

    __asm__ volatile ("cli");

    /* call hlt forever. the first call should halt the CPU indefinitely as interrupts are off, but just in case... */
    while (true) {
        __asm__ volatile ("hlt");
    }
}

/* build and return a GDT entry with the given base_address, limit_address, access and granularity flags */
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

void switch_to_idt(struct idt_pointer_struct* idt) {
    __asm__ volatile (
        "lidt (%0)\n\t"
        : 
        : "r"(idt) 
        : 
    );
}

struct idt_entry_struct idt_entry(uint32_t offset, uint16_t code_selector, uint8_t type_and_attributes) {
    struct idt_entry_struct entry;

    entry.offset_low_word = offset & 0xFFFF;
    entry.code_selector = code_selector;   
    entry.zero = 0;
    entry.type_and_attributes = type_and_attributes | 0x80;   
    entry.offset_high_word = (offset >> 16) & 0xFFFF;

    return entry;
}

void setup_idt() {
    uint32_t handlers[IDT_ENTRY_COUNT];

    idt.limit = sizeof(struct idt_entry_struct) * IDT_ENTRY_COUNT - 1;
    idt.base = (uint32_t)&idt_entries;
    memset(&idt_entries, 0, sizeof(idt_entries));

    START_INTERRUPT_LABEL(128);
        terminal_write("Syscall interrupt fired.\n");
        bochs_break();
    END_INTERRUPT_LABEL(128);

    /* software interrupts */
    idt_entries[0x80] = idt_entry(handlers[0x80], 0x8, 0xe);

    terminal_write("This IDT was built (first 8 entries):\n");
    terminal_hexdump(idt_entries, sizeof(struct idt_entry_struct) * 8);

    switch_to_idt(&idt);

    terminal_write("Calling test interrupt...\n");
    __asm__ volatile (
        "int $0x80"
    );     
}

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

    terminal_write("This GDT was built:\n");
    terminal_hexdump(gdt_entries, sizeof(struct gdt_entry_struct) * GDT_ENTRY_COUNT);

    switch_to_gdt(&gdt);
}
