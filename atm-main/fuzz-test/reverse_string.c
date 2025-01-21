#include <string.h>
#include <stdio.h>

#define MAX_LEN 100

void reverse_string(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

int main() {
    char input[MAX_LEN];

    // Read from stdin (AFL++ will provide input)
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return 1; // Handle empty input
    }

    // Remove newline character if present
    input[strcspn(input, "\n")] = '\0';

    reverse_string(input);
    printf("Reversed string: %s\n", input);

    return 0;
}
