#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "client.h"
#define TCP_PORT 5100

int main(int argc, char** argv){
    if(argc<2){
        printf("usage : %s IP_ADDR\n", argv[0]);
        return -1;
    }

    set_sigaction_sigchld();    // 좀비 프로세스를 비동기적으로 처리
    set_sigaction_sigusr1();    // 부모가 자식으로부터 종료 신호를 받으면 종료

    // 로그인
    char type[10];
    while(1){
        int select = sign_in_menu();
        switch (select) {
            case 1:
                strncpy(type, "SIGN_IN", 7);
                break;
            case 2:
                strncpy(type, "SIGN_UP", 7);
                break;
            case 3:
                exit(0);
            default:
                printf("Invalid select.\n");
                continue;
        }

        int client_status = CLIENT_ERROR;
        while(client_status == CLIENT_ERROR){
            char line[BUFSIZ];
            enter_id_and_pw(type, line);
            int server_sock = connect_to_server(argv);
            client_status = run_client(server_sock, line);
        } while (client_status == CLIENT_ERROR);
    }
}