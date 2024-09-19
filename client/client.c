#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "client.h"

// 시그널 핸들러 설정
void handle_sigchld(int sig){
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// 좀비 프로세스를 비동기적으로 처리
void set_sigaction_sigchld(){
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction_sigchld");
        return;
    }    
}

// 자식 -> 부모 종료 신호 설정
void handle_sigusr1(int sig){
    printf("Server disconnected.\n");
    exit(0);
}

// 부모가 자식으로부터 종료 신호를 받으면 종료
void set_sigaction_sigusr1(){
    struct sigaction sa2;
    sa2.sa_handler = handle_sigusr1;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa2, NULL) == -1) {
        perror("sigaction_sigusr1");
        return;
    }    
}

// -------------------

// 로그인 메뉴
int sign_in_menu(){
    int select;
    printf("===========\n");
    printf("1. Sign In\n");
    printf("2. Sign Up\n");
    printf("3. Quit\n");
    printf("===========\n");
    printf(">> "); 
    scanf("%d", &select); getchar();
    return select;
}

void enter_id_and_pw(const char* type, char* line){
    char id[USER_INFO_LEN], pw[USER_INFO_LEN];
                memset(id, 0, USER_INFO_LEN); memset(pw, 0, USER_INFO_LEN); memset(line, 0, BUFSIZ);
                printf("ID: "); fgets(id, USER_INFO_LEN, stdin); id[strcspn(id, "\n")] = '\0';
                printf("PW: "); fgets(pw, USER_INFO_LEN, stdin); pw[strcspn(pw, "\n")] = '\0';
                snprintf(line, BUFSIZ, "%s %s %s", type, id, pw);
}

// -------------------

// 서버와의 연결 시도
int connect_to_server(char** argv){
    int server_sock;
    struct sockaddr_in server_addr;
    // 소켓 생성
    if((server_sock = socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("socket");
        exit(1);  // 소켓 생성 실패 시 종료
    }

    // 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &(server_addr.sin_addr.s_addr));
    server_addr.sin_port = htons(TCP_PORT);

    // 서버 연결
    if(connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(server_sock);
        exit(1);  // 연결 실패 시 종료
    }

    return server_sock;
}

// 서버의 자식 프로세스에게 전달받은 메세지 출력
void print_message(int server_sock, int pipe_fd[2]){
    char buf[BUFSIZ];
    pid_t ppid = getppid();
    close(pipe_fd[0]);  // 부모 <- 자식 방향 설정
    while(1){
        memset(buf, 0, BUFSIZ);
        ssize_t len = recv(server_sock, buf, BUFSIZ, 0);
        if (len < 0){
            perror("recv");
            break;
        } else if(len == 0){
            kill(ppid, SIGUSR1);    // 서버 종료 시, 부모 프로세스에게 사욜자 설정 시그널 전달
            break;
        }
        // 로그인, 회원가입 요청에 대한 응답인 경우, 따로 처리
        if(strncmp(buf, "SIGN_IN_RESULT", 14) == 0 || strncmp(buf, "SIGN_UP_RESULT", 14) == 0){
            write(pipe_fd[1], buf, len);
        } else if(strncmp(buf, "INVALID", 7) == 0){
            write(pipe_fd[1], buf, len);
        } else{
            printf("%s\n", buf);
        }
        
    }
    close(server_sock);
    close(pipe_fd[1]);
    exit(0);
}

// 클라이언트 실행
int run_client(int server_sock, const char* line){
    // 파이프 생성
    int pipe_fd[2];
    if(pipe(pipe_fd) < 0){
        perror("pipe");
        close(server_sock);
        exit(1);
    }

    // 새로운 프로세스 생성
    pid_t pid = fork();
    if(pid < 0){
        perror("fork");
        close(server_sock);
        exit(1);
    }
    if(pid == 0){   // 자식 프로세스
        print_message(server_sock, pipe_fd);
    } else{         // 부모 프로세스
        send(server_sock, line, strlen(line), 0);   // 로그인,회원가입 요청 전송
        char pipe_buf[BUFSIZ];
        memset(pipe_buf, 0, BUFSIZ);
        ssize_t len = read(pipe_fd[0], pipe_buf, BUFSIZ);
        if(len < 0){
            perror("read");
            kill(pid, SIGTERM);
            close(server_sock);
            exit(1);
        } else if(len == 0){
            printf("Pipe disconnected.\n");
            kill(pid, SIGTERM);
            close(server_sock);
            exit(0);
        } else {
            if(strncmp(pipe_buf, "SIGN_IN_RESULT", 14) == 0){
                if(strchr(pipe_buf, '0') != NULL){
                    printf("Sign in failed.\n");
                    kill(pid, SIGTERM);
                    close(server_sock);
                    return CLIENT_ERROR;  // 로그인 실패 시 다시 입력으로
                } else if(strchr(pipe_buf, '1') != NULL){
                    if(strchr(pipe_buf, '-') != NULL){
                        printf("File not exists.\n");
                        return CLIENT_LOGOUT;   // users.txt 파일이 없으면 메뉴 화면으로
                    }
                    printf("----------VedaChat----------\n");
                }
            } else if(strncmp(pipe_buf, "SIGN_UP_RESULT", 14) == 0){
                if(strchr(pipe_buf, '0') != NULL){
                    printf("Sign up failed.\n");
                    kill(pid, SIGTERM);
                    close(server_sock);
                    return CLIENT_ERROR;  // 회원가입 실패 시 다시 입력으로
                }
                else{
                    printf("----------VedaChat----------\n");
                }
            } else{
                printf("Invalid input: ID or PW is empty.\n");
                kill(pid, SIGTERM);
                close(server_sock);
                return CLIENT_ERROR;  // 잘못된 입력 시 다시 입력으로
            } 
        }

        char buf[BUFSIZ];
        while(1){
            memset(buf, 0, BUFSIZ);
            fgets(buf, BUFSIZ, stdin);
            buf[strcspn(buf, "\n")] = '\0';
            // 종료
            if(strcmp(buf, "logout") == 0){
                kill(pid, SIGTERM);
                close(server_sock);
                return CLIENT_LOGOUT; // 로그아웃 시 메뉴 화면으로   
            }
            send(server_sock, buf, strlen(buf), 0);
        }
        close(server_sock);

    }
}
