#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "reverse_string.h"

#define MAX_LEN 100

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size >= MAX_LEN) return 0; // Ignore oversized inputs

    char input[MAX_LEN];
    memcpy(input, data, size);
    input[size] = '\0'; // Ensure null-terminated string

    reverse_string(input); // Process the fuzzed input
    return 0; // Indicate no crash
}
