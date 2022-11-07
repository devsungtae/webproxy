#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;                   // 클라 file descriptor
    char *host, *port, buf[MAXLINE];// host, port 포인터, buffer
    rio_t rio;                      // rio_t 구조체 : fd와 임시 buffer 정보

    if (argc != 3) {                // 인자를 2개 받아야 한다       /* argv[0]에는 항상 실행 경로가 들어간다. */
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1]; // host
    port = argv[2]; // port

    clientfd = Open_clientfd(host, port);   // clientfd를 연다
    // read buffer의 포맷을 초기화하는 함수
    Rio_readinitb(&rio, clientfd);      // ////////////////1개의 빈 buffer를 설정하고 1개의 open file descriptor(&rio)를 연결

    while(Fgets(buf, MAXLINE, stdin) != NULL) { // text줄을 반복해서 읽는 루프 진입
        Rio_writen(clientfd, buf, strlen(buf)); // buf의 크기만큼 buf에서 clientfd로 write한다
        Rio_readlineb(&rio, buf, MAXLINE);      // &rio에 있는 텍스트 라인 전체를 buf에 복사 (최대 MAXLINE만큼)
        Fputs(buf, stdout);                     // buf에 있는 텍스트를 stdout(표준출력)으로 보냄
    }
    Close(clientfd);                            // 루프 종료 후 식별자 닫음 -> 서버로 EOF 통지
    exit(0);                                    // 클라이언트 종료
                                                // 사실, 클라 커널이 프로세스가 종료할 때, 자동으로 열었던 모든 식별자들을 닫아줌
                                                // 원래는 close는 불필요하지만, 열었던 식별자를 명시적으로 닫는 것이 올바른 습관
}