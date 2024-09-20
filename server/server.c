#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include "server.h"

ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
int server_sock, client_sock;
struct sockaddr_in server_addr, client_addr;
socklen_t server_addr_len = sizeof(server_addr);
socklen_t client_addr_len = sizeof(client_addr);

// 시그널 핸들러 설정
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// 좀비 프로세스를 비동기적으로 처리
void set_sigaction_sigchld(){ 
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return;
    }
}

// 로그인
int sign_in(const char* id, const char* pw){
    // 중복 로그인 방지
    for(int i = 0; i < client_count - 1; i++){
        if(strcmp(clients[i].user_id, id) == 0){
            return 0;   // 아이디 중복, 로그인 실패
        }
    }
    
    FILE* file = fopen(FILE_NAME, "r");
    if(file == NULL){
        perror("fopen");
        return -1;  
    }
    char line[FILE_LINE_LEN];
    while(fgets(line, FILE_LINE_LEN, file)){
        char stored_id[USER_INFO_LEN], stored_pw[USER_INFO_LEN];
        sscanf(line, "%s %s", stored_id, stored_pw);
        if(strcmp(id, stored_id) == 0 && strcmp(pw, stored_pw) == 0){
            strcpy(clients[client_count - 1].user_id, id);  // 아이디 저장
            fclose(file);
            return 1;   // 아이디 & 비밀번호 중복, 로그인 성공
        }
    }
    fclose(file);
    return 0;   // 로그인 실패
}

// 회원가입
int sign_up(const char* id, const char* pw){
    FILE* file = fopen(FILE_NAME, "a+");
    if(file == NULL){
        perror("fopen");
        return -1;
    }
    char line[FILE_LINE_LEN];
    while(fgets(line, FILE_LINE_LEN, file)){
        char stored_id[USER_INFO_LEN];
        sscanf(line, "%s", stored_id);
        if(strcmp(id, stored_id) == 0){
            fclose(file);
            return 0;   // 아이디 중복, 회원가입 실패
        }
    }
    // 사용자 정보 추가
    fprintf(file, "%s %s\n", id, pw);
    fclose(file);
    strcpy(clients[client_count - 1].user_id, id);  // 아이디 저장
    return 1;   // 회원가입 성공
}

// 서버 생성
void set_server(){
    // 서버 소켓 생성
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(1);   
    }

    // 서버 소켓 동작 방식 설정 (SO_REUSEADDR = 주소 재사용 가능)
    int optval = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        close(server_sock);
        exit(1);
    }

    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(TCP_PORT);

    // 서버 소켓 바인딩
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(1);
    }

    // 연결 대기를 위한 큐 설정
    if (listen(server_sock, MAX_CLIENTS)) {
        perror("listen");
        close(server_sock);
        exit(1);
    }
}

// 논블록킹 모드 설정
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 2개의 파이프를 이용한 양방향 통신
void two_way_communication(int client_sock){
    char buf[BUFSIZ];
    while(1){
        memset(buf, 0, BUFSIZ); // 버퍼 초기화
        // 부모 <- 자식 <- 클라
        ssize_t len_1 = read(client_sock, buf, BUFSIZ);
        if(len_1 < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // 당장은 데이터가 없음, 다른 클라이언트를 확인해보자
            } else{
                perror("read from client");
                close(client_sock);
                exit(1);
            }
        } else if(len_1 == 0){
            close(client_sock);
            exit(0);
        } else{
            write(clients[client_count].from_child_pipe[1], buf, len_1);
        }

        // 부모 -> 자식 -> 클라
        ssize_t len_2 = read(clients[client_count].to_child_pipe[0], buf, BUFSIZ);
        if(len_2 < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // 당장은 데이터가 없음, 다른 클라이언트를 확인해보자
            } else{
                perror("read from parent");
                close(client_sock);
                exit(1);
            }
        } else if(len_2 == 0){
            printf("Parent pipe closed.\n");
            close(client_sock);
            exit(0);
        } else {
            write(client_sock, buf, len_2);
        }
    }
    close(client_sock);
    close(clients[client_count].to_child_pipe[0]);
    close(clients[client_count].from_child_pipe[1]);
}

// 입력받은 메세지에 대한 브로드 캐스트
void broadcast_message(){
    char buf[BUFSIZ];
    char new_buf[USER_INFO_LEN + BUFSIZ + 3];
    char status[10];
    for(int i = 0; i < client_count; i++){
        memset(buf, 0, BUFSIZ);    // 버퍼 초기화
        memset(new_buf, 0, USER_INFO_LEN + BUFSIZ + 3);
        // 부모 <- 자식
        ssize_t len = read(clients[i].from_child_pipe[0], buf, BUFSIZ);
        if(len < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                continue;
            } else{
                perror("read from child");
                continue;
            }
        } else if(len == 0){
            // 클라이언트 연결 해제시 구조체 배열 업데이트
            close(clients[i].to_child_pipe[1]);
            close(clients[i].from_child_pipe[0]);
            kill(clients[i].pid, SIGTERM);
            // 구조체 배열 한칸씩 앞으로
            for(int k = i; k < client_count - 1; k++){
                clients[k] = clients[k+1];
            }
            client_count--;
            i--;    // 제외 위치 기준으로 한칸씩 당겼으니 현재 위치도 한칸 앞으로
            printf("Total clients: %d\n", client_count);    // 접속 해제 후 클라이언트 수
        } else{
            // 원본 수정을 방지하기 위한 복사본 생성
            char tmp[BUFSIZ];
            strcpy(tmp, buf);
            // SIGN_IN, SIGN_UP이 포함된 메세지를 받으면 브로드 캐스트 중단 및 로그인 처리
            char* token = strtok(tmp, " ");
            if(strcmp(token, "SIGN_IN") == 0 || strcmp(token, "SIGN_UP") == 0){
                char *id = strtok(NULL, " "); char* pw = strtok(NULL, " ");
                if(id && pw ){
                    int result;
                    char message[BUFSIZ];   
                    if(strcmp(token, "SIGN_IN") == 0){
                        result = sign_in(id, pw);
                        snprintf(message, BUFSIZ, "SIGN_IN_RESULT %d", result);
                    } else{
                        result = sign_up(id, pw);
                        snprintf(message, BUFSIZ, "SIGN_UP_RESULT %d", result);
                    }
                    write(clients[i].to_child_pipe[1], message, strlen(message));
                } else{
                    char error_message[] = "INVALID_INPUT";
                    write(clients[i].to_child_pipe[1], error_message, strlen(error_message));
                }
            } else{
                snprintf(new_buf, USER_INFO_LEN + BUFSIZ + 3, "[%s] %s", clients[i].user_id, buf);
                for(int j = 0; j < client_count; j++){
                    // 입력한 자식 프로세스는 제외
                    if(j != i){
                        write(clients[j].to_child_pipe[1], new_buf, strlen(new_buf));   // 부모 -> 자식들
                    }
                }
            }
        }     
    }
}

void run_server(){
    while(1){
        // 클라이언트 소켓 생성
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if(client_sock < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // 당장은 데이터가 없음, 다른 클라이언트를 확인해보자
                // continue를 사용하면 안되는 이유: 새로운 클라이언트의 연결 시도가 없으면 continue로 인해 broadcast_message 부분을 건너뜀
            } else{
                perror("accept");
                close(client_sock);
                continue;
            }
        } else { 
            set_nonblocking(client_sock);   // 클라이언트 소켓도 이제 결과를 즉시 반환
            // 클라이언트 연결 최대치 제한
            if(client_count >= MAX_CLIENTS){
                printf("Max clients reached.\n");
                close(client_sock);
                continue;
            }
            // 양방향 파이프 생성 (논블록킹 모드 설정)
            pipe2(clients[client_count].to_child_pipe, O_NONBLOCK);
            pipe2(clients[client_count].from_child_pipe, O_NONBLOCK);

            // 새로운 프로세스 생성
            pid_t pid = fork();
            if(pid < 0){
                perror("fork");
                close(client_sock);
                continue;
            }
            if(pid == 0){  // 자식 프로세스
                close(server_sock);
                close(clients[client_count].to_child_pipe[1]);
                close(clients[client_count].from_child_pipe[0]);
                two_way_communication(client_sock); // 양방향 통신
            } else if(pid > 0){  // 부모 프로세스
                close(clients[client_count].to_child_pipe[0]);
                close(clients[client_count].from_child_pipe[1]);
                // 클라이언트 구조체 배열에 정보 추가
                clients[client_count].socket = client_sock;
                clients[client_count].pid = pid;
                client_count++;
                close(clients[client_count].to_child_pipe[1]);
                close(clients[client_count].from_child_pipe[0]);
                printf("Total clients: %d\n", client_count);    // 접속 후 클라이언트 수 
            }   
        }
        broadcast_message();    // 입력받은 메세지에 대한 브로드 캐스트
    }
    close(server_sock);
}
