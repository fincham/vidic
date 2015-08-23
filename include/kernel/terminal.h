#ifndef KERNEL_TERMINAL_HEADER
#define KERNEL_TERMINAL_HEADER

#include <stddef.h>
#include <stdint.h>

uint8_t vga_color(uint8_t foreground, uint8_t background);
uint16_t vga_character(char character);
uint16_t vga_cursor_memory_index();
void vga_move_cursor(uint8_t row, uint8_t column);

void terminal_clear();
void terminal_write(char *string);
void terminal_hexdump(void *memory, size_t byte_count);

#endif
