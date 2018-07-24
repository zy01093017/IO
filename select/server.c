#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#define INIT -1
//select特点：监视多个文件描述符的状态，当有一个或多个文件描述符状态发生改变，表示有数据可以读或者有缓冲区可以写数据
//用户告诉操作系统，要关心读数据还是写数据，当关心的文件描述符的事件发生改变，操作系同会告知用户，可以读或者可以写
//函数：int select(int fds, fd_set *readfds, fd_set* writefds, fd_set* execptfds, struct timeval* timeout);
//参数：nfds是最大文件描述符加1？
//readfds: 集合，是输入输出型参数，告诉操作系统关心哪些文件描述符，当其中的部分文件描述符事件已经就绪，操作系统就会返回给用户
//timeout：设置为NULL，表示没有超时时间，一致被阻塞，直到有时间了，才返回
//         设置为0，检测文件描述符集合的状态，立即返回
//         特定时间值，在指定时间里没有事件就返回

//返回值：大于0，表示就绪事件的个数
//        等于0，表示超时返回
//        小于0，表示出错

//1. 创建socket套接字
//2. 定义数组，用来保存放到select当中的fd，当select返回时，就可以判断我之前关心的是哪个源文件描述符


int Start(int port)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0){
    perror("socket");
    exit(2);
  }

  int opt = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in local;
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  local.sin_port = htons(port);

  if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0){
    perror("bind");
    exit(3);
  }

  if(listen(sock, 5) < 0){
    perror("listen");
    exit(4);
  }

  return sock;
}

void handlerRequest(fd_set* rfds, int fd_array[], int num)
{
  int i = 0;
  for(; i < num; i++ ){
    if(fd_array[i] == INIT){
      continue;

    }

    if(i == 0 && FD_ISSET(fd_array[i], rfds)){
      //get a new connect
      struct sockaddr_in client;
      socklen_t len = sizeof(client);
      int sock = accept(fd_array[i], (struct sockaddr*)&client,\
          &len);
      if(sock < 0){
        perror("accept");
        continue;
      }
      printf("get a new client...!\n");
      int j = 1;
      for(; j < num; j++){
        if(fd_array[j] == INIT){
          break;
        }
      }
      if(j < num){
        fd_array[j] = sock;
      }
      else{
        close(sock);
        printf("select is full!\n");
      }
    }
    else if( i != 0 && FD_ISSET(fd_array[i], rfds)  ){
      //normal read ready

      char buf[10240];
      ssize_t s = read(fd_array[i], buf, sizeof(buf)-1);
      if(s > 0){
        buf[s] = 0;
        printf("client# %s\n", buf);
      }else if( s == 0  ){
        close(fd_array[i]);
        printf("client quit!\n");
        fd_array[i] = INIT;
      }else{
        perror("read");
      }
    }
    else{
      //other
    }
  }
}



// ./select_server 80
int main(int argc, char* argv[])
{
  if(argc != 2){
    printf("Usage: [%s] port\n",argv[0]);
  } 
  //用于监听哪些文件描述符时间已经就绪
  int listen_sock = Start(atoi(argv[1]));//创建socket

  //1.等待文件描述符事件就绪
  //2.将自己要关心的事件添加到select当中
  //3.定义数组，来存放要关心的文件描述符，当select返回时，就知道有哪些文件描述符已经就绪
  int fd_array[sizeof(fd_set)*8];

  fd_array[0] = listen_sock;//当连接来了表示listen_sock已经就绪，此时表示读事件已经就绪
  int num = sizeof(fd_set)*8;
  int i = 1;
  for(; i < num; ++i)//初始化数组
  {
    fd_array[i] = INIT;
  }

  fd_set rfds;

  while(1){

    int maxfd = -1;
    //每次使用输入输出型参数时都需要进行初始化
    for( i = 0; i < num; ++i)
    {
      if(fd_array[i] == INIT)
        continue;

      FD_SET(fd_array[i], &rfds);//将有效的文件描述符设置进去读文件描述符集合
      if(maxfd < fd_array[i])
        maxfd = fd_array[i];
    }

    FD_ZERO(&rfds);
    FD_SET(listen_sock, &rfds);


    //struct timeval time = {0,0};

    switch(select(maxfd+1, &rfds, NULL,NULL, NULL)){
      case -1://出错
        perror("select");
        break;
      case 0://超时
        printf("timeout");
        break;
      default://处理请求
        handlerRequest(&rfds,fd_array,num);
        break;
    }
  }

  return 0;
}
































