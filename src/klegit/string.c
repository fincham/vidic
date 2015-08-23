#include <stddef.h>

/* XXX it is not clear that this implementation of memcpy works */
void* memcpy(void *destination_pointer, const void *source_pointer, size_t length) {
    unsigned char* destination = (unsigned char*) destination_pointer;
    const unsigned char* source = (const unsigned char*) source_pointer;

    for (size_t index; index < length; index++) {
        destination[index] = source[index];
    }

    return destination_pointer;
}
