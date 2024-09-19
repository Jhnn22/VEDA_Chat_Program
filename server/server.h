#ifndef _SERVER_H
#define _SERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#define TCP_PORT 5100
#define MAX_CLIENTS 50
#define USER_INFO_LEN 30
#define FILE_LINE_LEN 100
#define FILE_NAME "users.txt"

// 클라이언트 구조체 배열 선언
typedef struct {
    int socket;
    int to_child_pipe[2];
    int from_child_pipe[2];
    pid_t pid;
    char user_id[USER_INFO_LEN];
} ClientInfo;

// 중복 정의 방지, 전역 변수의 공유
extern ClientInfo clients[MAX_CLIENTS];
extern int client_count;
extern int server_sock, client_sock;
extern struct sockaddr_in server_addr, client_addr;
extern socklen_t server_addr_len, client_addr_len;

// 시그널
void handle_sigchld(int sig);
void set_sigaction_sigchld();

// 로그인, 회원가입
int sign_in(const char* id, const char* pw);
int sign_up(const char* id, const char* pw);

// 서버
void set_server();
void set_nonblocking(int fd);
void two_way_communication(int client_sock);
void broadcast_message();
void run_server();

#endif