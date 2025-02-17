#include "bank.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

Bank *bank_create()
{
    Bank *bank = (Bank *)malloc(sizeof(Bank));
    if (bank == NULL)
    {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state
    bank->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&bank->rtr_addr, sizeof(bank->rtr_addr));
    bank->rtr_addr.sin_family = AF_INET;
    bank->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&bank->bank_addr, sizeof(bank->bank_addr));
    bank->bank_addr.sin_family = AF_INET;
    bank->bank_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->bank_addr.sin_port = htons(BANK_PORT);
    bind(bank->sockfd, (struct sockaddr *)&bank->bank_addr, sizeof(bank->bank_addr));

    // Set up the protocol state
    // TODO set up more, as needed

    return bank;
}

void bank_free(Bank *bank)
{
    if (bank != NULL)
    {
        close(bank->sockfd);
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, data_len, 0,
                  (struct sockaddr *)&bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
}


typedef struct User
{
    char name[251];    // Username (max 250 chars + null)
    char pin[5];       // 4-digit PIN (plus null terminator)
    int balance;       // Account balance
    struct User *next; // Linked list pointer (if using linked list)
} User;

#define MAX_USERS 100
User *users[MAX_USERS];

// Simple hash function to distribute users in the table
int hash_function(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % MAX_USERS;
}

// Find a user by name
User *find_user(const char *name)
{
    int index = hash_function(name);
    User *u = users[index];
    while (u && strcmp(u->name, name) != 0)
    {
        u = u->next;
    }
    return u;
}

// Deposit money into a user's account
void deposit(char *name, int amount)
{
    if (!name || amount < 0)
    {
        printf("Usage: deposit <user-name> <amt>\n");
        return;
    }

    User *user = find_user(name);
    if (!user)
    {
        printf("No such user\n");
        return;
    }

    // Prevent integer overflow
    if (user->balance + amount < user->balance)
    {
        printf("Too rich for this program\n");
        return;
    }

    user->balance += amount;
    printf("$%d added to %s's account\n", amount, name);
}

// Check a user's balance
void check_balance(char *name)
{
    if (!name)
    {
        printf("Usage: balance <user-name>\n");
        return;
    }

    User *user = find_user(name);
    if (!user)
    {
        printf("No such user\n");
        return;
    }

    printf("$%d\n", user->balance);
}

void create_user(char *name, char *pin, int balance)
{
    if (!name || !pin || balance < 0 || strlen(name) > 250 || strlen(pin) != 4)
    {
        printf("Usage: create-user <user-name> <pin> <balance>\n");
        return;
    }

    if (find_user(name))
    {
        printf("Error: user %s already exists\n", name);
        return;
    }

    // Create new user in memory
    User *new_user = (User *)malloc(sizeof(User));
    strcpy(new_user->name, name);
    strcpy(new_user->pin, pin);
    new_user->balance = balance;
    new_user->next = NULL;

    // Insert into hash table
    int index = hash_function(name);
    new_user->next = users[index];
    users[index] = new_user;

    // Create user card file
    char filename[260];
    snprintf(filename, sizeof(filename), "%s.card", name);
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        printf("Error creating card file for user %s\n", name);
        free(new_user);
        return;
    }
    fprintf(f, "%s\n", pin); // Store the PIN in the card
    fclose(f);

    printf("Created user %s\n", name);
}


void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    char cmd[20], user[251];
    int amount;

    sscanf(command, "%s", cmd);

    printf("Received command: %s\n", command);
    if (strcmp(cmd, "check-user") == 0)
    {
        if (sscanf(command, "%*s %250s", user) != 1)
        {
            bank_send(bank, "Usage: check-user <user-name>", 30);
            return;
        }

        User *account = find_user(user);
        if (!account)
        {
            bank_send(bank, "No such user", 12);
        }
        else
        {
            bank_send(bank, "User exists", 11);
        }
        return;
    }

    // Handle balance check request
    else if (strcmp(cmd, "balance") == 0)
    {
        if (sscanf(command, "%*s %250s", user) != 1)
        {
            bank_send(bank, "Usage: balance <user-name>", 26);
            return;
        }

        User *account = find_user(user);
        if (!account)
        {
            bank_send(bank, "No such user", 12);
            return;
        }

        char response[50];
        snprintf(response, sizeof(response), "$%d", account->balance);
        bank_send(bank, response, strlen(response));
    }
    // Handle withdraw request
    else if (strcmp(cmd, "withdraw") == 0)
    {
        if (sscanf(command, "%*s %250s %d", user, &amount) != 2 || amount < 0)
        {
            bank_send(bank, "Usage: withdraw <user-name> <amt>", 34);
            return;
        }

        User *account = find_user(user);
        if (!account)
        {
            bank_send(bank, "No such user", 12);
            return;
        }

        if (account->balance < amount)
        {
            bank_send(bank, "Insufficient funds", 18);
            return;
        }

        account->balance -= amount;
        char response[50];
        snprintf(response, sizeof(response), "$%d dispensed", amount);
        bank_send(bank, response, strlen(response));
    }
    else
    {
        bank_send(bank, "Invalid command", 15);
    }
    /*
     * The following is a toy example that simply receives a
     * string from the ATM, prepends "Bank got: " and echoes
     * it back to the ATM before printing it to stdout.
     */

    /*
    char sendline[1000];
    command[len]=0;
    sprintf(sendline, "Bank got: %s", command);
    bank_send(bank, sendline, strlen(sendline));
    printf("Received the following:\n");
    fputs(command, stdout);
    */
}


void bank_process_local_command(Bank *bank, char *command, size_t len)
{
    char cmd[20], name[251], pin[5];
    int amount;

    sscanf(command, "%s", cmd);

    if (strcmp(cmd, "create-user") == 0)
    {
        if (sscanf(command, "%*s %250s %4s %d", name, pin, &amount) != 3)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }
        create_user(name, pin, amount);
    }
    else if (strcmp(cmd, "deposit") == 0)
    {
        if (sscanf(command, "%*s %250s %d", name, &amount) != 2)
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;
        }
        deposit(name, amount);
    }
    else if (strcmp(cmd, "balance") == 0)
    {
        if (sscanf(command, "%*s %250s", name) != 1)
        {
            printf("Usage: balance <user-name>\n");
            return;
        }
        check_balance(name);
    }
    else
    {
        printf("Invalid command\n");
    }
}
