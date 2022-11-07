// Iterative echo server main routine. 반복 에코 서버 메인 루틴, "iterative 서버"-한 번에 한 개씩의 클라를 반복하서 실행하는 서버 
// 한 번에 한 개의 클라만 처리할 수 있음
#include "csapp.h"
#include "echo.c"

void echo(int connfd);

int main(int argc, char **argv)
{
    int listenfd, connfd;   // 듣기 식별자, 연결 식별자
    socklen_t clientlen;    // ????????????????
    // accept로 보내지는 소켓 주소 구조체
    struct sockaddr_storage clientaddr; /* Enough space for any address */// sockaddr_storage구조체 -> 모든 형태의 소켓 주소를 저장하기에 충분히 큼 -> 프로토콜 독립적으로 유지함
    char client_hostname[MAXLINE], client_port[MAXLINE];    // 클라 주소, 포트

    if (argc != 2) {                        // 입력이 하나만 들어와야 함
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);      // 듣기 식별자 open
    while(1) {                              // 루프 진입
        clientlen = sizeof(struct sockaddr_storage);    // 클라 길이 ?????????????
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);           // 클라로부터 연결요청 기다리기 ??????
        // accept가 리턴하기 전에 clientaddr에는 클라의 소켓 주소가 들어감
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);//???????
        printf("Connected to (%s, %s)\n", client_hostname, client_port);    // 클라이언트 이름, 포트 출력
        echo(connfd);                                                       // echo함수 호출 -> 클라이언트 서비스
        Close(connfd);                                                      // 연결식별자 닫기
    }
    exit(0);                                                                // 클라와 서버가 식별자를 닫은 후 종료
}