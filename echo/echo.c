#include "csapp.h"

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];  // 버퍼
    rio_t rio;

    // 텍스트 파일을 표준입력에서 출력으로, 한 번에 한 줄씩 Rio함수를 이용해서 복사
    Rio_readinitb(&rio, connfd);    // 연결 식별자를 읽기버퍼(&rio에 위치)와 연결 
    // Rio_readlineb : &rio에 있는 텍스트 라인 전체를 buf(내부 read buffer)에 복사하는 wrapper함수, 최대 MAXLINE-1 바이트를 읽음
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {   // Rio_readlineb 함수가 EOF를 만날 때까지 text줄 반복해서 읽고 써줌
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n); // n바이트를 buf에서 connfd로 write한다
    }
}
