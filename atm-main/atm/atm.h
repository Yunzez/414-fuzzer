/*
 * The ATM interfaces with the user.  User commands should be
 * handled by atm_process_command.
 *
 * The ATM can read .card files and the .atm init file, but not any
 * other files you want to create.
 *
 * Feel free to update the struct and the processing as you desire
 * (though you probably won't need/want to change send/recv).
 */

#ifndef __ATM_H__
#define __ATM_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

typedef struct _ATM
{
    // Networking state
    int sockfd;
    struct sockaddr_in rtr_addr;
    struct sockaddr_in atm_addr;

    // Protocol state
    // TODO add more, as needed
    char current_user[251];  // Stores the name of the logged-in user
    int is_authenticated;    // 1 if logged in, 0 if not
} ATM;

ATM* atm_create();
void atm_free(ATM *atm);
ssize_t atm_send(ATM *atm, char *data, size_t data_len);
ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len);
void atm_process_command(ATM *atm, char *command);

#endif
