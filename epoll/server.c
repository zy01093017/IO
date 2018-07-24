#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

//用户想关心一个事件，就创建一个节点，将其添加到红黑树当中去，并且构建映射关系，当事件就绪后，就使用回调
//机制将其添加到就绪队列当中，用户每次从就绪队列当中拿到就绪事件进行操作
//问题：红黑树只是用来存放事件的吗？

int Start(int port)
{
  int sock = socket(AF_INET,SOCK_STREAM,0);
  if(sock < 0){
    perror("socket");
    exit(2);
  }

  struct sockaddr_in local;
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  local.sin_port = htons(port);

  if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0){
    perror("bind");
    exit(3);
  }

  if(listen(sock,5) < 0){
    perror("listen");
    exit(4);
  }

  return sock;
}


void handlerReadyEvents(int epfd, struct epoll_event revs[], int num, int listen_sock)
{
  int i = 0;
  struct epoll_event ev;
  for(; i < num; ++i){
    int fd = revs[i].data.fd;
    uint32_t events = revs[i].events;

    if(listen_sock == fd && (events & EPOLLIN)){//表示监听到了新连接
      //将其事件添加到红黑树中，还没有就绪时间
      //1.先获取新连接 2.在将文件描述符进行添加
      int sock = accept(fd, NULL,NULL);
      if(sock < 0){
        perror("accept");
        continue;
      }
      printf("get new accept...\n");
      ev.data.fd = sock;
      ev.events = EPOLLIN;

      epoll_ctl(epfd,EPOLL_CTL_ADD,sock,&ev);
    }
    else if(events & EPOLLIN){//读事件就绪
      char buf[4096];
      ssize_t s = read(fd, buf, sizeof(buf)-1);
      if(s < 0){
        perror("read");
      }
      else if(s == 0){//对端关闭
        printf("client quit...\n");
        close(fd);
        epoll_ctl(epfd,EPOLL_CTL_MOD, fd, &ev);
      }
      else 
      {
        buf[s] = 0;
        printf("%s\n",buf);
        
        ev.data.fd = fd;//服务器要写回去，此时要关心写事件什么时候就绪
        ev.events = EPOLLOUT;
        epoll_ctl(epfd,EPOLL_CTL_ADD,fd,NULL);
      }
    }
    else if(events & EPOLLOUT){//写事件就绪
      const char* echo = "HTTP/1.0 200 OK\r\n\r\n<html><hl>hello Epoll server:)</hl></html>\r\n";
      write(fd,echo,strlen(echo));
      close(fd);
      epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
    }
    else{//其他异常事件
    }
  }
}
int main(int argc, char* argv[])
{
  if(argc != 2){
    printf("Usage: %s [port]\n",argv[0]);
    return 1;
  }

  //创建套接字，用来监听是否有就绪事件
  int listen_sock = Start(atoi(argv[1]));
  
  //注册epoll模型
  int epfd = epoll_create(256);
  if(epfd < 0){
    perror("epoll_create");
    exit(5);
  }

  //将listen_sock添加到红黑树中，建立回调关系
  
  struct epoll_event event;
  event.events = EPOLLIN;//关心读
  event.data.fd = listen_sock;

  epoll_ctl(epfd,EPOLL_CTL_ADD, listen_sock, &event);

  struct epoll_event revs[128];//返回的事件
  int timeout = -1;//超时时间
  for(; ; ){
    int num = epoll_wait(epfd,revs,128,timeout);
    if(num < 0){
      perror("epoll_wait");
      exit(6);
    }
    else if(num == 0){
      printf("timeout....\n");
      continue;
    }

    //表示有事件就绪，可以开始处理
    handlerReadyEvents(epfd,revs,num,listen_sock);

  }


  return 0;
}



