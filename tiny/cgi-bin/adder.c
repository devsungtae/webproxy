/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  // /* Extract the two arguments */
  // if ((buf = getenv("QUERY_STRING")) != NULL) { // 인자 전달된 경우
  //   p = strchr(buf, '&');   // strchr: 문자열 내에 일치하는 문자가 있는지 검사, '&'가 존재하면 해당 포인터 반환
  //   *p = '\0';              // '&'를 '\0'로 바꾸기
  //   strcpy(arg1, buf);      // 첫번째 인자를 arg1에 
  //   strcpy(arg2, p+1);      // 두번째 인자를 arg2에
  //   n1 = atoi(arg1);        // 문자를 정수로
  //   n2 = atoi(arg2);
  // }
  printf("adder 들어옴\n");


  if ((buf = getenv("QUERY_STRING")) != NULL) { // 인자 전달된 경우
    p = strchr(buf, '&');   // strchr: 문자열 내에 일치하는 문자가 있는지 검사, '&'가 존재하면 해당 포인터 반환
    *p = '\0';              // '&'를 '\0'로 바꾸기
    sscanf(buf, "num1=%d", n1);
    sscanf(p+1, "num2=%d", n2);
    printf("n1: %d, n2: %d", n1, n2);

  }  

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
