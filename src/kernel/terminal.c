#include <stddef.h>
#include <stdint.h>

#include <kernel/intel.h>
#include <klegit/mini-printf.h>

static const size_t VGA_HEIGHT = 25;
static const size_t VGA_WIDTH = 80;
static const size_t HEXDUMP_WIDTH = 8;

static const uint8_t VGA_FOREGROUND_COLOUR = 10; /* standard grey */
static const uint8_t VGA_BACKGROUND_COLOUR = 5; /* blue */

static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

uint8_t cursor_row = 0;
uint8_t cursor_column = 0;

/* produce a colour value suitable for VGA character memory */
uint8_t vga_color(uint8_t foreground, uint8_t background)
{
    return foreground | background << 4;
}

/* produce two bytes to be written in to VGA memory to set a character, foreground and background color */
uint16_t vga_character(char character) {
    return character | vga_color(VGA_FOREGROUND_COLOUR, VGA_BACKGROUND_COLOUR) << 8;
}

uint16_t vga_cursor_memory_index() {
    return cursor_row * VGA_WIDTH + cursor_column;
}

void vga_move_cursor(uint8_t column, uint8_t row) {
    cursor_row = row;
    cursor_column = column;

    /* wrap if the right-most edge of the screen is reached */
    if (cursor_column > VGA_WIDTH - 1) {
        cursor_row++;
        cursor_column = 0;
    }

    /* scroll the screen if the cursor has fallen off the bottom XXX make this use memcpy once memcpy works */
    if (cursor_row > VGA_HEIGHT - 1) {
        cursor_row = VGA_HEIGHT - 1;

        /* scroll up VGA memory by one line */
        for (size_t index = 0; index < VGA_HEIGHT * VGA_WIDTH + VGA_WIDTH; index++) {
            VGA_MEMORY[index] = VGA_MEMORY[index + VGA_WIDTH];
        }

        /* blank the freshly exposed bottom line */
        for (size_t index = 0; index < VGA_WIDTH; index++) {
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + index] = vga_character(' ');
        }
    }

    /* poke the VGA ports to move the cursor XXX strictly the base port should be queried from the BIOS not hardcoded */
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(vga_cursor_memory_index() & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char )((vga_cursor_memory_index() >> 8) & 0xFF));
 }

/* clear the terminal and move the cursor back to the top left */
void terminal_clear() {
    for (uint8_t row = 0; row < VGA_HEIGHT; row++) {
        for (uint8_t column = 0; column < VGA_WIDTH; column++) {
            VGA_MEMORY[row * VGA_WIDTH + column] = vga_character(' ');
        }
    }
    vga_move_cursor(0, 0);
}

/* write a null terminated string at the current cursor position. handles \n for newlines. */
void terminal_write(char *string) {
    for (size_t index = 0; string[index] != 0; index++) {
        if (string[index] == '\n') {
            vga_move_cursor(0, ++cursor_row);
        } else {
            VGA_MEMORY[vga_cursor_memory_index()] = vga_character(string[index]);
            vga_move_cursor(++cursor_column, cursor_row);
        }
    }
}

/* dump an area of memory as hex to the terminal XXX fix to use "printf" rather than buffers + snprintf */
void terminal_hexdump(void *memory, size_t byte_count) {
    char buffer[256];

    for (size_t index = 0; index < byte_count + ((byte_count % HEXDUMP_WIDTH) ? (HEXDUMP_WIDTH - byte_count % HEXDUMP_WIDTH) : 0); index++) {
        /* print offset at line start */
        if (index % HEXDUMP_WIDTH == 0) {
            mini_snprintf(buffer, 256, "0x%06x: ", index);
            terminal_write(buffer);
        }

        /* print hex data */
        if (index < byte_count) {
            mini_snprintf(buffer, 256, "%02x ", 0xFF & ((char*)memory)[index]);
            terminal_write(buffer);                       
        } else {
            terminal_write("   ");                                              
        }

        if (index % HEXDUMP_WIDTH == HEXDUMP_WIDTH - 1) {
            terminal_write("\n");
        }
    }
}
