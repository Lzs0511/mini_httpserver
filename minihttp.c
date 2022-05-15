#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<stdlib.h>
#include<ctype.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<error.h>
#include<errno.h>
#include<pthread.h>

#define TRUE 1
#define BUF_SIZE 256
struct stat statbuf;
char path[512];
static int debug = 0;
int get_line(int, char *, int );
void *do_http_request(void *);
void do_http_response(int clnt_sock);
void not_found(int );
void not_implemented(int clnt_sock);
void inner_error(int clnt_sock);
void bad_request(int clnt_sock);
int send_head(int clnt_sock, FILE *resource);
void send_body(int clnt_sock, FILE *resource);
int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Usage : %s <Port> \n", argv[0]);
        exit(1);
    }
    // 1. 创建套接字
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    // 编写服务器端地址信息
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof saddr);
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));

    // 2.bind套接字与地址
    if(bind(sock, (struct sockaddr *)&saddr, sizeof saddr) == -1){
        perror("bind");
        exit(2);
    }

    // 3.监听连接
    if(listen(sock, 128) == -1){
        perror("listen");
        exit(3);
    }
    // 创建客户端通信套接字和地址信息
    int clnt_sock;
    struct sockaddr_in clnt_addr;
    pthread_t tid;
    socklen_t clnt_sz = sizeof(clnt_addr);
    while(TRUE){
        clnt_sock = accept(sock, (struct sockaddr *)&clnt_addr, &clnt_sz);
        if(clnt_sock == -1){
            perror("accept");
            exit(4);
        }
        printf("New Client Connected...\n");
        pthread_create(&tid, NULL, do_http_request, &clnt_sock);
        pthread_join(tid, NULL);
        
    }

    return 0;
}

void *do_http_request(void *sock){
    int clnt_sock = *(int *)sock;
    int len = 0;
    char buf[BUF_SIZE];
    char method[64];
    char url[BUF_SIZE];
    // 读取客户端发送的http请求
    int i = 0, j = 0;
    len = get_line(clnt_sock, buf, BUF_SIZE);
    if(len > 0){
        
        while(!isspace(buf[j]) && i < sizeof(method) - 1){
            method[i++] = buf[j++];
        }
        method[i] = 0;
        if(debug) printf("request method: %s\n", method);

        if(!strncasecmp(method, "GET", strlen(method))){
            // 只处理GET请求
            if(debug) printf("method = GET\n");
            i = 0;
            // 跳过空格
            while(isspace(buf[j]))j++;
            // 获取url
            while(!isspace(buf[j]) && i < sizeof(url)-1){
                url[i++] = buf[j++];
            }

            url[i] = 0;
            
            if(debug) printf("url: %s\n", url);
            // 继续读取http头部
            while(get_line(clnt_sock, buf, BUF_SIZE) > 0){
                if(debug) printf("request line: %s\n", buf);
            }

            // 定位服务器本地的html文件
            // 处理url中的?
            char *pos = strchr(url, '?');
            if(pos){
                // 有问号, 将问号替换为字符串结束符
                *pos = '\0';
                printf("read url: %s\n", url);
            }
            sprintf(path, "./html%s", url);
            printf("path : %s\n", path);

            // 判断文件是否存在
            if(stat(path, &statbuf) == -1){
                fprintf(stderr, "stat %s failed. reason: %s\n", path, strerror(errno));
                not_found(clnt_sock);
                close(clnt_sock);
            }else {
                // 文件存在
                if(S_ISDIR(statbuf.st_mode)){
                    strcat(path, "/index.html");
                }
                do_http_response(clnt_sock);
    
            }
            // 执行http 响应

        }else{
            // 非GET请求, 读取Http头部，并响应客户端 501
            fprintf(stderr, "Warning! other request [%s]\n", method);
            while(get_line(clnt_sock, buf, BUF_SIZE) > 0){
                if(debug) printf("request line: %s\n", buf);
            }
            not_implemented(clnt_sock); // 响应时再实现
        }

        
    }else{
        // bad request
        bad_request(clnt_sock);
    }
    close(clnt_sock);
    return NULL;
}
void not_found(int clnt_sock){
    const char * reply = "HTTP/1.0 404 NOT FOUND\r\n"
        "Content-Type: text/html\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "<html lang=\"zh-CN\">\n"
        "<head>\n"
        "<meta content=\"text/html\"; charset=\"utf-8\" http-equiv=\"Content-Type\">\n"
        "<title>This is a test</title>\n"
        "</head>\n"
        "<body>\n"
        "<div height=\"500px\">\n"
        "<br/><br/>\n"
        "<h2>404 NOT FOUND</h2>\n"
        "</div>\n"
        "</body>\n";
    int len = write(clnt_sock, reply, strlen(reply));
    if(debug) fprintf(stdout,"reply is: %s", reply);
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void not_implemented(int clnt_sock){
    const char * reply = "HTTP/1.0 501 Method Not Implement\r\n"
        "Content-Type: text/html\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "<html lang=\"zh-CN\">\n"
        "<head>\n"
        "<meta content=\"text/html\"; charset=\"utf-8\" http-equiv=\"Content-Type\">\n"
        "<title>This is a test</title>\n"
        "</head>\n"
        "<body>\n"
        "<div height=\"500px\">\n"
        "<br/><br/>\n"
        "<h2>501 Method Not Implement</h2>\n"
        "</div>\n"
        "</body>\n";
    int len = write(clnt_sock, reply, strlen(reply));
    if(debug) fprintf(stdout,"reply is: %s", reply);
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}
void bad_request(int clnt_sock){
    const char * reply = "HTTP/1.0 400 Bad Request\r\n"
        "Content-Type: text/html\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "<html lang=\"zh-CN\">\n"
        "<head>\n"
        "<meta content=\"text/html\"; charset=\"utf-8\" http-equiv=\"Content-Type\">\n"
        "<title>This is a test</title>\n"
        "</head>\n"
        "<body>\n"
        "<div height=\"500px\">\n"
        "<br/><br/>\n"
        "<h2>400 Bad Request</h2>\n"
        "</div>\n"
        "</body>\n";
    int len = write(clnt_sock, reply, strlen(reply));
    if(debug) fprintf(stdout,"reply is: %s", reply);
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}
void inner_error(int clnt_sock){
    const char * reply = "HTTP/1.0 500 NOT FOUND\r\n"
        "Content-Type: text/html\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "<html lang=\"zh-CN\">\n"
        "<head>\n"
        "<meta content=\"text/html\"; charset=\"utf-8\" http-equiv=\"Content-Type\">\n"
        "<title>This is a test</title>\n"
        "</head>\n"
        "<body>\n"
        "<div height=\"500px\">\n"
        "<br/><br/>\n"
        "<h2>500 服务器异常</h2>\n"
        "</div>\n"
        "</body>\n";
    int len = write(clnt_sock, reply, strlen(reply));
    if(debug) fprintf(stdout,"reply is: %s", reply);
    if(len <= 0){
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}


void do_http_response(int clnt_sock){
    int ret;
    if(debug) printf("do_http_response start...\n");
    FILE *resource = NULL;
    resource = fopen(path, "r");
    if(resource == NULL){
        not_found(clnt_sock);
        return ;
    }
    // 1. 发送HTTP头部
    ret = send_head(clnt_sock, resource);
    
    // 2. 发送HTTP响应体
    if(!ret)
        send_body(clnt_sock, resource);
    fclose(resource);
}

int send_head(int clnt_sock, FILE *resource){
    struct stat st;
    int fd = 0;
    char tmp[64] = {0};
    char buf[1024] = {0};
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    strcat(buf, "Server: KpsilentServer\r\n");
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "Connection: Close\r\n");
    fd = fileno(resource);

    if(fstat(fd, &st) == -1){
        // 出错
        inner_error(clnt_sock);
        return -1;
    }else{
        snprintf(tmp, 64, "Content-Length: %ld\r\n\r\n", st.st_size);
        strcat(buf, tmp);
    }
    // 查看响应头
    if(debug) fprintf(stdout, "response head: \n%s", buf);
    
    if(send(clnt_sock, buf, strlen(buf), 0) < 0){
        fprintf(stderr, "send failed. data: %s, reason: %s\n", buf, strerror(errno));
        return -1;
    }
    return 0;
}
// 发送html
void send_body(int clnt_sock, FILE *resource){
    char buf[1024];

    while(fgets(buf, sizeof buf, resource)){
        int len = write(clnt_sock, buf, strlen(buf));
        if(len < 0){
            // 发送过程中出现问题
            fprintf(stderr, "send body error. reason: %s\n", strerror(errno));
            break;
        }
        if(debug) fprintf(stdout, "%s", buf);
    }
}

int get_line(int sock, char *buf, int size){
    int count = 0;
    char ch = '\0';
    int len = 0;

    while(count < size - 1 && ch != '\n'){
        len = read(sock, &ch, 1);
        if(len == 1){
            if(ch == '\r')
                continue;
            else if(ch == '\n')
                break;
            buf[count++] = ch;
        }else if(len == -1){
            // read出错
            perror("read");
            count = -1;
            break;
        }else {
            // len == 0, 客户端关闭了连接
            fprintf(stderr, "client close...\n");
            count = -1;
            break;
        }
    }
    // count 为-1表示读取出错, 等于0时表示读到一个空行，大于0表示读取一行
    if(count >= 0)buf[count] = 0;
    return count;
}
