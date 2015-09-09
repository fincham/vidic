#include <stddef.h>

void* memcpy(void *destination_pointer, const void *source_pointer, size_t length) {
    unsigned char* destination = (unsigned char*) destination_pointer;
    const unsigned char* source = (const unsigned char*) source_pointer;

    for (size_t index = 0; index < length; index++) {
        destination[index] = source[index];
    }

    return destination_pointer;
}
