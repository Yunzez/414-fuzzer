#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void error_exit(const char *message, int code) {
    fprintf(stderr, "%s\n", message);
    exit(code);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        error_exit("Usage: init <filename>", 62);
    }

    char bank_file[256], atm_file[256];
    snprintf(bank_file, sizeof(bank_file), "%s.bank", argv[1]);
    snprintf(atm_file, sizeof(atm_file), "%s.atm", argv[1]);

    // Check if the files already exist
    FILE *f;
    if ((f = fopen(bank_file, "r"))) {
        fclose(f);
        error_exit("Error: one of the files already exists", 63);
    }
    if ((f = fopen(atm_file, "r"))) {
        fclose(f);
        error_exit("Error: one of the files already exists", 63);
    }

    // Create the bank file
    f = fopen(bank_file, "w");
    if (!f) error_exit("Error creating initialization files", 64);
    fprintf(f, "# Bank initialization file\n");
    fclose(f);

    // Create the ATM file
    f = fopen(atm_file, "w");
    if (!f) {
        remove(bank_file);  // Rollback
        error_exit("Error creating initialization files", 64);
    }
    fprintf(f, "# ATM initialization file\n");
    fclose(f);

    printf("Successfully initialized bank state\n");
    return 0;
}
