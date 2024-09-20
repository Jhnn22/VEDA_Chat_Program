
#include "server.h"

int main(int argc, char** argv) {
    demon();                        // 서버 데몬으로 실행
    set_sigaction_sigchld();        // 좀비 프로세스를 비동기적으로 처리
    set_server(server_sock);        // 서버 소켓 생성 및 초기화
    set_nonblocking(server_sock);   // 서버 소켓은 이제 결과를 즉시 반환 
    run_server();                   // 서버 실행


    return 0;
}