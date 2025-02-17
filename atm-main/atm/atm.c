#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ATM *atm_create()
{
    ATM *atm = (ATM *)malloc(sizeof(ATM));
    if (atm == NULL)
    {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state
    atm->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&atm->rtr_addr, sizeof(atm->rtr_addr));
    atm->rtr_addr.sin_family = AF_INET;
    atm->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&atm->atm_addr, sizeof(atm->atm_addr));
    atm->atm_addr.sin_family = AF_INET;
    atm->atm_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->atm_addr.sin_port = htons(ATM_PORT);
    bind(atm->sockfd, (struct sockaddr *)&atm->atm_addr, sizeof(atm->atm_addr));

    // Set up the protocol state
    // TODO set up more, as needed
    // Initialize state
    atm->is_authenticated = 0;
    memset(atm->current_user, 0, sizeof(atm->current_user));

    return atm;
}

void atm_free(ATM *atm)
{
    if (atm != NULL)
    {
        close(atm->sockfd);
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, data_len, 0,
                  (struct sockaddr *)&atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
}

int authenticate_user(ATM *atm, const char *user_name)
{
    char filename[260];
    snprintf(filename, sizeof(filename), "%s.card", user_name);

    // Step 1: Ask the bank if the user exists
    char request[300];
    snprintf(request, sizeof(request), "check-user %s", user_name);
    atm_send(atm, request, strlen(request) + 1);

    // Step 2: Receive response from bank
    char response[100];
    int n = atm_recv(atm, response, sizeof(response));
    if (n > 0)
    {
        response[n] = '\0';
        if (strcmp(response, "No such user") == 0)
        {
            printf("No such user\n");
            return 0;
        }
    }

    // Step 3: Check if the card file exists
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        printf("Unable to access %s's card\n", user_name);
        return 0;
    }

    // Step 4: Read the stored PIN
    char stored_pin[5];
    if (fscanf(f, "%4s", stored_pin) != 1)
    {
        fclose(f);
        printf("Invalid card file\n");
        return 0;
    }
    fclose(f);

    // Step 5: Ask the user for their PIN
    printf("PIN? ");
    char entered_pin[5];
    if (scanf("%4s", entered_pin) != 1)
    {
        printf("Not authorized\n");
        return 0;
    }

    // Step 6: Consume extra input to prevent "Invalid command"
    while (getchar() != '\n')
        ;

    // Step 7: Compare the PINs
    if (strcmp(stored_pin, entered_pin) == 0)
    {
        printf("Authorized\n");
        return 1;
    }
    else
    {
        printf("Not authorized\n");
        return 0;
    }
}

void atm_process_command(ATM *atm, char *command)
{
    char cmd[20], user[251] = {0};
    int amount;

    // Trim any trailing newline
    command[strcspn(command, "\n")] = 0;

    sscanf(command, "%19s", cmd);

    if (strcmp(cmd, "begin-session") == 0)
    {
        if (sscanf(command, "%*s %250s", user) != 1)
        {
            printf("Usage: begin-session <user-name>\n");
            return;
        }
        printf("Extracted user: %s\n", user); // Debug log to verify correct parsing

        if (atm->is_authenticated)
        {
            printf("A user is already logged in\n");
            return;
        }

        if (!authenticate_user(atm, user))
        {
            return;
        }

        strcpy(atm->current_user, user);
        atm->is_authenticated = 1;
    }

    else if (strcmp(cmd, "balance") == 0)
    {
        if (!atm->is_authenticated)
        {
            printf("No user logged in\n");
            return;
        }

        char request[300];
        snprintf(request, sizeof(request), "balance %s", atm->current_user);

        // Ensure correct termination
        request[sizeof(request) - 1] = '\0';

        printf("Sending request to bank: %s\n", request);
        atm_send(atm, request, strlen(request) + 1); // Include null terminator

        char response[100];
        int n = atm_recv(atm, response, sizeof(response));
        if (n > 0)
        {
            response[n] = '\0';
            printf("%s\n", response);
        }
    }

    else if (strcmp(cmd, "withdraw") == 0)
    {
        if (!atm->is_authenticated)
        {
            printf("No user logged in\n");
            return;
        }

        if (sscanf(command, "%*s %d", &amount) != 1 || amount < 0)
        {
            printf("Usage: withdraw <amt>\n");
            return;
        }

        char request[300];
        snprintf(request, sizeof(request), "withdraw %s %d", atm->current_user, amount);
        atm_send(atm, request, strlen(request));

        char response[100];
        int n = atm_recv(atm, response, sizeof(response));
        if (n > 0)
        {
            response[n] = '\0';
            printf("%s\n", response);
        }
    }

    else if (strcmp(cmd, "end-session") == 0)
    {
        if (!atm->is_authenticated)
        {
            printf("No user logged in\n");
            return;
        }

        atm->is_authenticated = 0;
        memset(atm->current_user, 0, sizeof(atm->current_user));

        printf("User logged out\n");
    }

    else
    {
        printf("Invalid command\n");
    }

    /*
     * The following is a toy example that simply sends the
     * user's command to the bank, receives a message from the
     * bank, and then prints it to stdout.
     */

    /*
    char recvline[10000];
    int n;

    atm_send(atm, command, strlen(command));
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    fputs(recvline,stdout);
    */
}
