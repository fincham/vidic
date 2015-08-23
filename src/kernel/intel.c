#include <stdbool.h>

#include <kernel/intel.h>
#include <kernel/terminal.h>

#define GDT_ENTRY_COUNT 6

gdt_entry_t gdt_entries[GDT_ENTRY_COUNT];
tss_t tss;
gdt_pointer_t gdt;

/* write a byte to an IO port */
void outb(unsigned int port, unsigned char byte) {
   __asm__ volatile ("outb %%al, %%dx" : : "d" (port), "a" (byte));
}

/* halt the CPU in a way that hopefully doesn't cause it to catch fire */
void halt() {
    terminal_write("Halting CPU.");

    __asm__("cli");

    /* call hlt forever. the first call should halt the CPU indefinitely as interrupts are off, but just in case... */
    while (true) {
        __asm__("hlt");
    }
}

/* build and return a GDT entry with the given base_address, limit_address, access and granularity flags */
gdt_entry_t gdt_entry(uint32_t base_address, uint32_t limit_address, uint8_t access_flags, uint8_t granularity_flags) {
    gdt_entry_t entry;

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
void switch_to_gdt(gdt_pointer_t *gdt) {
    /*
    0x08 = the first entry in the GDT
    0x10 = the second entry
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

void setup_gdt() {
    gdt.limit = (sizeof(gdt_entry_t) * GDT_ENTRY_COUNT - 1);
    gdt.base = (uint32_t)&gdt_entries;

    /* construct a basic GDT which maps all of memory */
    gdt_entries[0] = gdt_entry(0, 0, 0, 0);
    gdt_entries[1] = gdt_entry(0, 0xFFFFFFFF, 0x9A, 0xC0); /* ring 0 code segment */
    gdt_entries[2] = gdt_entry(0, 0xFFFFFFFF, 0x92, 0xC0); /* ring 0 data segment */
    gdt_entries[3] = gdt_entry(0, 0xFFFFFFFF, 0xFA, 0xC0); /* ring 3 code segment */
    gdt_entries[4] = gdt_entry(0, 0xFFFFFFFF, 0xF2, 0xC0); /* ring 3 code segment */
    gdt_entries[5] = gdt_entry((uint32_t)&tss, sizeof(tss), 0x89, 0xC0); /* TSS for later multitasking */

    terminal_write("This GDT was built:\n");
    terminal_hexdump(gdt_entries, sizeof(gdt_entry_t) * GDT_ENTRY_COUNT);

    switch_to_gdt(&gdt);
}
