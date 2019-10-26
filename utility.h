#ifndef UTILITY_H
#define UTILITY_H


#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <time.h>

#define SERVER_PORT 21
#define BUF_SIZE 1024
#define PATH_SIZE 256

enum COMMAND
{
    USER,PASS,RETR,STOR,QUIT,SYST,TYPE,PORT,PASV,MKD,CWD,PWD,LIST,RMD,RNFR,RNTO,OTHER
};

enum CLIENTSTATE
{
    NOTLOGGED,NEEDPASS,LOGGED
};

enum TRANSMODEL
{
    UNKNOWN,PORTMODEL,PASVMODEL
};

struct client{
    int commandfd;
    int data_fd;
    int pasv_listening_fd;
    enum CLIENTSTATE state;  
    char ip_addr[32];
    int port_trans;
    enum TRANSMODEL trans_model;
    char name_prefix[PATH_SIZE]; //绝对路径
    enum COMMAND last_cmd;
    char file_rnfr[PATH_SIZE]; 

};


//解析命令行，得到参数port和root，p_port 和 root 被修改
void parse_argument(int* p_port, char* root, int argc, char **argv);

//这个函数从msg获得正确格式的lastline_response
int make_response(int code, char* msg, char* lastline_response);

//初始化一个客户的信息
void initialize_client(struct client* new_client, char* root);

//解析从客户传来的命令commandline，并得到（如果有的话）参数
int parse_cmdline(char* commandline, char* param);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int user_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int pass_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int port_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int pasv_cmd(char* param, struct client* m_client);

//返回新建立的socket，同时 m_client->data_fd 也被赋值为新的socket，失败返回-1
int port_connect(struct client* m_client);
 
//类上
int pasv_connect(struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int retr_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int stor_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int type_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int quit_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int pwd_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int cwd_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int mkd_cmd(char* param, struct client* m_client);

//返回0：删除失败  返回1：删除成功
int rmd_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int rnfr_cmd(char* param, struct client* m_client);

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int rnto_cmd(char* param, struct client* m_client);
#endif