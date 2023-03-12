#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <arpa/inet.h>

#define SERV_PORT 9527
#define INADDR_ANY ((in_addr_t) 0x00000000)


void sys_err(const char *str) {
    perror(str);
    exit(1);
}

int main(int argc, char *argv[]) {
    int lfd = 0, cfd = 0;
    int ret;                                        // 读到的字节数
    char buf[BUFSIZ], clit_IP[1024];                // 缓存区，客户端IP存放
    
    struct sockaddr_in serv_addr, clit_addr;        // 服务端，客户端套接字
    socklen_t clit_addr_len;                        // 客户端套接字长度
    serv_addr.sin_family = AF_INET;                 // 配置监听阶段套接字 IP4协议
    serv_addr.sin_port = htons(SERV_PORT);          // 端口号
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // IP地址

    lfd = socket(AF_INET, SOCK_STREAM, 0);          // 初始化监听阶段套接字
    if (lfd == -1) {
        sys_err("socket error !");
    }

    bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));    // 将套接字与IP端口绑定
    
    listen(lfd, 128);                                               // 配置最大连接数

    clit_addr_len = sizeof(clit_addr);
    cfd = accept(lfd, (struct sockaddr *)&clit_addr, &clit_addr_len);
    if (cfd == -1) {
        sys_err("accept eror !");
    }

    printf("client ip %s, port: %d\n", 
        inet_ntop(AF_INET, &clit_addr.sin_addr.s_addr, clit_IP, sizeof(clit_IP)),
        ntohs(clit_addr.sin_port));

    while (1) {
        ret = read(cfd, buf, sizeof(buf));      // read
        
        for (int i = 0; i < ret; i++) {         // toupper
            buf[i] = toupper(buf[i]);
        }
        
        write(cfd, buf, ret);                   // write
    }

    close(lfd);
    close(cfd);
    
    return 0;
}