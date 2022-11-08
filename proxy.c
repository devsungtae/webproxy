#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
// https://developer.mozilla.org/ko/docs/Glossary/Request_header
static const char *request_hdr_format = "%s %s HTTP/1.0\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *Accept_hdr = "    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *EOL = "\r\n";

// 함수
void doit(int connfd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri,char *hostname,char *path,int *port);
int make_request(rio_t* client_rio, char *hostname, char *path, int port, char *hdr, char *method);
void *thread(void *vargp);  // Pthread_create 에 루틴 반환형이 정의되어있음
void sigpipe_handler(int sig);

int main(int argc, char **argv) { // tiny : 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결요청을 듣는다
  int listenfd, *clientfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;                // ????
  struct sockaddr_storage clientaddr; // ????
  pthread_t tid;  // peer 스레드에 부여할 tid 번호 (unsigned long)

  /* Check command line args */
  if (argc != 2) {  // 주소를 올바르게 입력해야 함
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, sigpipe_handler);

  listenfd = Open_listenfd(argv[1]);      // 입력받은 포트에 듣기 소켓을 연다
  while (1) {                             // 무한 서버 루프 실행
    clientlen = sizeof(clientaddr);
    clientfd = (int *)Malloc(sizeof(int));   // 여러개의 디스크립터를 만들 것이므로 덮어쓰지 못하도록 고유메모리에 할당 // 반복적으로 연결요청 접수 
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 프록시가 서버로서 클라이언트와 맺는 파일 디스크립터(소켓 디스크립터) : 고유 식별되는 회선이자 메모리 그 자체

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                    0); // print accepted message
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // sequential handle the client transaction
    //doit(clientfd);   // line:netp:tiny:doit    // 트랜잭션 수행
    //Close(clientfd);  // line:netp:tiny:close   // 자신 쪽의 connection endpoint 닫기
    Pthread_create(&tid, NULL, thread, clientfd);
    printf("\n=====disconnect=====\n");
  }
}

void *thread(void *argptr) {
  int clientfd = *((int *)argptr);
  Pthread_detach((pthread_self()));
  Free(argptr);
  Signal(SIGPIPE, sigpipe_handler);
  doit(clientfd);
  Close(clientfd);
  return NULL;
}

void sigpipe_handler(int sig) {
    printf("SIGPIPE handled\n");
    return;
}

/*
 * doit : proxy에서 서버로 요청 보내기
 */
void doit(int client_fd)
{
  int endserver_fd;  // 클라이언트fd, 엔드서버fd
  char hostname[MAXLINE], path[MAXLINE];
  int port;    //
  char buf[MAXLINE], hdr[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];   

  // rio_t 구조체 : fd와 임시 buffer 정보
  rio_t client_rio, endserver_rio; // 클라 요청 받는 rio, 서버 요청 보내는 rio
  
  // 클라이언트 요청 읽기
  Rio_readinitb(&client_rio, client_fd);      // 클라rio랑 클라fd 연결
  Rio_readlineb(&client_rio, buf, MAXLINE);  // 클라buf에 클라rio내용 넣기
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  // 클라의 요청에서 method, uri, version 나누기

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    // 501 요청은 프록시 선에서 처리
    printf("[PROXY]501 ERROR\n");
    clienterror(client_fd, method, "501", "Not Implemented",
          "Tiny does not implement this method");
    return;
  }

  parse_uri(uri, hostname, path, &port);  // req uri 파싱하여 hostname, path, port(포인터) 뱐수에 할당
  printf("[out]hostname=%s port=%d path=%s\n", hostname, port, path);
  if (!strlen(hostname)) {
    clienterror(client_fd, method, "501", "No Hostname",
                "Hostname is necessary");
  }
  if (!make_request(&client_rio, hostname, path, port, hdr, method)) {
    clienterror(client_fd, method, "501", "request header error",
              "Request header is wrong");      
  }

  char port_value[100];
  sprintf(port_value, "%d", port);  // port를 port_value에 저장
  endserver_fd = Open_clientfd(hostname, port_value);   // 서버와의 소켓 디스크립터 생성 - 파싱한 hostname과 port_value로

  Rio_readinitb(&endserver_rio, endserver_fd);      // 엔드서버 소켓과 연결 - 엔드서버rio랑 엔드서버fd 연결
  Rio_writen(endserver_fd, hdr, strlen(hdr));     // 엔드서버에 request 보냄 - endserver에게 client buffer 전달

  int n;
  while ((n = Rio_readlineb(&endserver_rio, buf, MAXLINE)) > 0) {
    Rio_writen(client_fd, buf, n);   // 클라이언트에게 응답 전달
  }
  Close(endserver_fd);
}

/*
 * clienterror : 에러 메시지를 클라이언트에게 보냄
 */
 /* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */  // response body -> 에러를 설명, HTML 파일도 함께 보냄
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n", 
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */ // HTTP response를 응답 라인에 상태 코드와 상태 메시지와 함께 클라에게 보냄
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
 /* $end clienterror */

/*
 * parse_uri: uri를 알맞게 파싱해서 hostname, port, path로 나누어 준다
 */
void parse_uri(char *uri, char *hostname, char *path, int *port)
{
  /*
    uri가  
    / , /cgi-bin/adder 이렇게 들어올 수도 있고,
    http://11.22.33.44:5001/home.html 이렇게 들어올 수도 있다.
    알맞게 파싱해서 hostname, port로, path 나누어주어야 한다!
  */

  *port = 80; // web port 80으로 고전
  
  printf("uri=%s\n", uri);

  char *parsed;
  parsed = strstr(uri, "//"); // '//' 뒤로 쭉 있는 문자열의 가장 앞 포인터

  if (parsed == NULL) {
    parsed = uri; // uri가 null임
  }
  else {
    parsed = parsed + 2;  // 포인터 두 칸 이동 -> '//'의 뒤쪽인 ':'부터 시작
  }

  char *parsed2 = strstr(parsed, ":");  // ':'를 가리키는 포인터
  
  if (parsed2 == NULL) {  // port가 없는 경우 -> 경로는 있을 수 있음 filename
    // ':' 이후가 없다면 port가 없음
    parsed2 = strstr(parsed, "/"); 
    if (parsed2 == NULL) {  // 포트 뒤 경로가 없는 경우
      sscanf(parsed, "%s", hostname); // hostname에 parsed를 넣기
    }
    else {  // 경로가 있는 경우
      printf("parsed=%s parsed2=%s\n", parsed, parsed2);
      *parsed2 = '\0';  /////////////////????????????? 왜 있지??????
      sscanf(parsed,"%s",hostname); // hostname에 parsed 넣기
      *parsed2 = '/';
      sscanf(parsed2,"%s",path);    // path에 parsed2 넣기
    }
  }
  else {
    // ':' 이후가 있으므로 port가 있음
    *parsed2 = '\0';
    sscanf(parsed, "%s", hostname);
    sscanf(parsed2+1, "%d%s", port, path);
  }
  printf("hostname=%s port=%d path=%s\n", hostname, *port, path);
}

/*
 * make_request : 프록시 서버로 들어온 요청을 서버에 전달하기 위해 HTTP 헤더 생성
 */
int make_request(rio_t* client_rio, char *hostname, char *path, int port, char *hdr, char *method) 
{
  char req_hdr[MAXLINE], additional_hdf[MAXLINE], host_hdr[MAXLINE];
  char buf[MAXLINE];
  char *HOST = "Host";
  char *CONN = "Connection";
  char *UA = "User-Agent";
  char *P_CONN = "Proxy-Connection";
  sprintf(req_hdr, request_hdr_format, method, path); // method url version

  while (1) {
    if (Rio_readlineb(client_rio, buf, MAXLINE) == 0) break;
    if (!strcmp(buf,EOL)) break;  // buf == EOL => EOF

    if (!strncasecmp(buf, HOST, strlen(HOST))) {
      // 호스트 헤더 지정
      strcpy(host_hdr, buf);
      continue;
    }

    if (strncasecmp(buf, CONN, strlen(CONN)) && strncasecmp(buf, UA, strlen(UA)) && strncasecmp(buf, P_CONN, strlen(P_CONN))) {
       // 미리 준비된 헤더가 아니면 추가 헤더에 추가 
      strcat(additional_hdf, buf);  
    }
  }

  if (!strlen(host_hdr)) {
    sprintf(host_hdr, host_hdr_format, hostname);
  }

  sprintf(hdr, "%s%s%s%s%s%s", 
    req_hdr,   // METHOD URL VERSION
    host_hdr,   // Host header
    user_agent_hdr,
    connection_hdr,
    proxy_connection_hdr,
    EOL
  );
  if (strlen(hdr))
    return 1;
  return 0;

}