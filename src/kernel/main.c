#include <stddef.h>
#include <stdint.h>

#include <kernel/intel.h>
#include <kernel/terminal.h>

/* entry point from early.S - at this point there is a 32k stack set up, but nothing else */
void main() {
    terminal_clear();
    terminal_write("Called kernel main().\n\n");

    terminal_write("Setting up the GDT...\n");
    setup_gdt();
    terminal_write("Installed GDT.\n\n");

    terminal_write("Setting up the IDT...\n");
    setup_idt();
    terminal_write("Installed IDT.\n");

    halt();

    /* for (size_t index = 0; index < 4294967295; index++) {
        terminal_write("This is a loop ");
        terminal_hexdump(&index, sizeof(index));
    } */    
}
