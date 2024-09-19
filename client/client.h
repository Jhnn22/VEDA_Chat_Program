#ifndef _CLIENT_H
#define _CLIENT_H

#include <sys/socket.h>
#include <arpa/inet.h>
#define TCP_PORT 5100
#define USER_INFO_LEN 30
// 클라이언트 상태 정의
#define CLIENT_ERROR -1
#define CLIENT_LOGOUT 0

// 시그널
void handle_sigchld(int sig);
void set_sigaction_sigchld();
void handle_sigusr1(int sig);
void set_sigaction_sigusr1();

// 로그인
int sign_in_menu();
void enter_id_and_pw(const char* type, char* line);

// 클라이언트
int connect_to_server(char** argv);
void print_message(int server_sock, int pipe_fd[2]);
int run_client(int server_sock, const char* line);

#endif