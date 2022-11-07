/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) { // tiny : 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결요청을 듣는다
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;                // ????
  struct sockaddr_storage clientaddr; // ????

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);      // 듣기 소켓을 연다
  while (1) {                             // 무한 서버 루프 실행
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,  // 반복적으로 연결요청 접수
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit    // 트랜잭션 수행
    Close(connfd);  // line:netp:tiny:close   // 자신 쪽의 connection endpoint 닫기
    printf("\n=====disconnect=====\n");
  }
}

/*
 * doit : 한 개의 HTTP 트랜잭션을 처리
 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  // strcasecmp(s1, s2) : s1, s2 대소문자를 구분하지 않고 비교
  // s1 > s2 return 양수
  // s1 = s2 return 0
  // s1 < s2 return 음수
  if (strcasecmp(method, "GET")) {                  // tiny는 GET 메소드만 받음
    clienterror(fd, method, "501", "Not implemented", 
             "Tiny does not implement this method");
             return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);    // 요청이 정적 컨텐츠인지 동적 컨텐츠인지 확인
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found",   // 파일이 디스크에 없으면 클라에게 에러메시지 보냄
                "Tiny couldn't read the file");
    return;                                         // 리턴
  }
  if (is_static) { /* Serve static content */       // 정적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  // 보통 파일인지, 읽기권한 있는지
      clienterror(fd, filename, "403", "Forbidden", 
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);       // 정적 컨텐츠 클라에게 제공
  }
  else {  /* Serve dynamic content */               // 동적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 파일이 실행 가능한지 검증  
      clienterror(fd, filename, "403", "Forbidden", 
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);           // 동적 컨텐츠 클라에게 제공
  }
}

/*
 * clienterror : 에러 메시지를 클라이언트에게 보냄
 */
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

/*
 * read_requesthdrs : 요청 헤더를 읽고 무시한다. 
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {  // 요청 헤더를 종료하는 빈 텍스트 줄
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*
 * parse_uri : HTTP URI를 분석
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{ // 정적 컨텐츠를 위한 홈 디렉토리 = 현재 디렉토리, 실행파일의 홈 디렉토리 = /cgi-bin
  char *ptr;
  // 
  if (!strstr(uri, "cgi-bin")) {  /* Static content */ // /cgi-bin을 포함하는 모든 URI는 동적 컨텐츠 요청이라고 가정
    strcpy(cgiargs, "");          // CGI 인자 스트링을 지운다 
    strcpy(filename, ".");        // URI를 ./index.html 같은 상대 리눅스 경로이름으로 변환
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')    // URI가 '/'문자로 끝나면
      strcat(filename, "home.html");  // 기본 파일 이름(home.html)을 추가함
    return 1;
  }
  else {  /* Dynamic content */ // 동적 컨텐츠 요청인 경우  
    ptr = index(uri, '?');      //  | 모든 CGI 인자들을 추출
    if (ptr) {                  //  |
      strcpy(cgiargs, ptr+1);   //  |
      *ptr = '\0';              //  |
    }                           //  |
    else                        //  |
      strcpy(cgiargs, "");      //  | 모든 CGI 인자들을 추출
    strcpy(filename, ".");  // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환
    strcat(filename, uri);  //
    return 0;
  }
}

/*
 * serve_static : 정적 컨텐츠를 클라이언트에게 서비스함
 */
void serve_static(int fd, char *filename, int filesize)
{ // 지원하는 정적 컨텐츠 타입 5개 : HTML file, 무형식 text file, GIF, PNG, JPEG로 인코딩된 영상
  int srcfd;  
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);                         // 파일 타입 알아내기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                      // 클라에게 응답 줄과 응답 헤더를 보냄
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // %s -> buf 나타냄
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);// 빈 줄 한 개가 헤더를 종료함 ////////////
  Rio_writen(fd, buf, strlen(buf));                         // buf에 있는 요청한 파일의 내용을 연결식별자 fd로 복사
  printf("Response headers:\n");
  printf("%s", buf);    // buf 출력

  /* Send response body to client */  // response body를 클라에게 보냄
  srcfd = Open(filename, O_RDONLY, 0);// O_RDONLY -> 파일을 읽기 전용으로 오픈 // 읽기 위해 filename open하고 식별자 얻어옴
  // void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
  // mmap() -> fd로 지정된 파일에서 offset을 시작으로 length바이트만큼 start주소로 대응시킴. -> 요청한 파일을 가상메모리 영역으로 매핑
  // Mmap함수 -> 호출 시, 파일 srcfd의 첫번째 filesize 바이트를 주소 srcp에서 시작하는 private 읽기-허용 가상메모리 영역으로 매핑
  // prot -> 메모리보호모드 설정, PROT_READ: 페이지는 읽을 수 있다
  // flags -> 현재 프로세스에서만 보일 건지 다른 프로세스와 공유할건지
  // srcfd 파일 전체를 가상메모리 영역으로 매핑하고 해당 포인터를 srcp에 저장
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 가상메모리 영역 srcp 만들고, 거기에 fd파일 내용을 대응시킴
  srcp = (char *)malloc(filesize); // srcp에 filesize만큼 할당
  Rio_readn(srcfd, srcp, filesize);// readn: 식별자에서 메모리로 전송 // srcfd를 srcp와 매핑 -> srcfd에 있는 내용을 srcp에 복사

  Close(srcfd);       // 매핑 후에는 식별자가 필요 없으니 파일을 닫는다
  Rio_writen(fd, srcp, filesize);// writen: 메모리에서 식별자로 전송 // 주소 srcp에서 시작하는 filesize바이트를 클라의 연결식별자로 복사
  //Munmap(srcp, filesize);           // 매핑된 가상메모리 주소를 반환 -> 메모리 누수 피하기
  free(srcp); // Munmap 대신 사용
}

/*
 * get_filetype : Derive file type from filename
 */
 void get_filetype(char *filename, char *filetype)
 {
  if (strstr(filename, ".html"))   // strstr() : 문자열 안에 특정 문자열이 있는지 탐색해주는 함수
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))      //////
    strcpy(filetype, "video/mp4");        ////// [[[[수정함]]]]
  else                        
    strcpy(filetype, "text/plain");
 }

/*
 * serve_dynamic : 동적콘텐츠를 클라이언트에 제공
 */
 void serve_dynamic(int fd, char *filename, char *cgiargs)  // Tiny는 child process를 fork하고, 그 후 CGI프로그램을 child의 context에서 실행함, 모든 종류 동적 컨텐츠 제공
 {
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 클라이언트에 성공을 알려주는 응답라인 보내기
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  if (Fork() == 0) {  /* Child */ // child 프로세스 fork
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // child는 QUERY_STRING 환경변수를 요청 URI의 CGI 인자들로 초기화한다
    Dup2(fd, STDOUT_FILENO);   /* Redirect stdout to client */ // 자식의 표준 출력을 연결파일 식별자로 재지정
    Execve(filename, emptylist, environ); /* Run CGI program */ // CGI 프로그램 로드하고 실행
      // CGI program이 child context에서 실행됨 -> execve 함수를 호출하기 전에 존재하던 열린 파일들과 환경변수들에도 동일하게 접근 가능
      // -> CGI program이 표준 출력에 쓰는 모든 것은 직접 클라이언트 프로세스로 부모 프로세스의 간섭 없이 전달됨 
  }
  Wait(NULL); /* Parent waits for and reaps child */  // 부모는 자식이 종료되어 정리되는 것을 기다림
 }