#include "utility.h"

char my_server_ip[] = "127.0.0.1";

//解析命令行，得到参数port和root，p_port 和 root 被修改
void parse_argument(int* p_port, char* root, int argc, char **argv){
	strcpy(root,"/tmp");
	for (int i = 0; i < argc; i++){
		if (strcmp(argv[i],"-port") == 0)
		{
			*p_port = atoi(argv[i+1]);
			i++;
		}else if (strcmp(argv[i],"-root") == 0){
			memset(root, 0, PATH_SIZE);
			strcpy(root, argv[i+1]);
			i++;
		}
	}
}

//这个函数从msg获得正确格式的lastline_response
//ftp要求response的最后一行是： begins with three ASCII digits and a space
int make_response(int code, char* msg, char* lastline_response){
    char tmp[BUF_SIZE] = {0};
    memset(lastline_response, 0, sizeof(lastline_response));
    strcpy(tmp,msg);
    lastline_response[2] = (code%10) + '0';
    code /= 10;
    lastline_response[1] = (code%10) + '0';
    code /= 10;
    lastline_response[0] = code + '0';

    lastline_response[3] = ' ';
    for (int i = 0; i < strlen(tmp); i++){
        lastline_response[4+i] = tmp[i];
    }
    lastline_response[strlen(lastline_response)] = '\r';
    //已测过，确实strlen会增长1
    lastline_response[strlen(lastline_response)] = '\n';
    return strlen(lastline_response);
    
}


//初始化一个客户的信息
void initialize_client(struct client* new_client, char* root){
    new_client->commandfd = -1;
    new_client->data_fd = -1;
    new_client->pasv_listening_fd = -1;
    new_client->state = NOTLOGGED;  
    memset(new_client->ip_addr, 0, sizeof(new_client->ip_addr));
    new_client->port_trans = 0;
    new_client->trans_model = UNKNOWN;
    memset(new_client->name_prefix, 0, sizeof(new_client->name_prefix));
    strcpy(new_client->name_prefix, root);
    memset(new_client->file_rnfr, 0, sizeof(new_client->file_rnfr));
}


//解析从客户传来的命令commandline，并得到（如果有的话）参数
int parse_cmdline(char* commandline, char* param){
    char* cmd = strtok(commandline, " ");
    char* tmp_param = strtok(NULL, " ");
    int cmd_len = strlen(cmd);
    enum COMMAND cmd_e;
    //当命令没有参数时，会读取到长度为6的cmd，因为有\r\n
    for(int i = 0; i < cmd_len; i++)
    {
        if ( cmd[i] =='\r' || cmd[i] == '\n'){
            cmd[i] = 0;
        }else{
            cmd[i] = toupper(cmd[i]);
        }
    }
    cmd_len = strlen(cmd);
    printf("cmd:%s\n", cmd);

    if (tmp_param)
    {
        int param_len = strlen(tmp_param);
        for(int i = 0; i < param_len; i++)
        {
            if ( tmp_param[i] =='\r' || tmp_param[i] == '\n'){
                tmp_param[i] = 0;
            }    
        }
        strcpy(param,tmp_param);
    }

    printf("param:%s\n", param);

    if (strcmp(cmd,"USER") == 0){
        cmd_e = USER;
    }else if (strcmp(cmd,"PASS") == 0){
        cmd_e = PASS;
    }else if (strcmp(cmd,"RETR") == 0){
        cmd_e = RETR;
    }else if (strcmp(cmd,"STOR") == 0){
        cmd_e = STOR;
    }else if (strcmp(cmd,"QUIT") == 0){
        cmd_e = QUIT;
    }else if (strcmp(cmd,"SYST") == 0){
        cmd_e = SYST;
    }else if (strcmp(cmd,"TYPE") == 0){
        cmd_e = TYPE;
    }else if (strcmp(cmd,"PORT") == 0){
        cmd_e = PORT;
    }else if (strcmp(cmd,"PASV") == 0){
        cmd_e = PASV;
    }else if (strcmp(cmd,"MKD") == 0){
        cmd_e = MKD;
    }else if (strcmp(cmd,"CWD") == 0){
        cmd_e = CWD;
    }else if (strcmp(cmd,"PWD") == 0){
        cmd_e = PWD;
    }else if (strcmp(cmd,"LIST") == 0){
        cmd_e = LIST;
    }else if (strcmp(cmd,"RMD") == 0){
        cmd_e = RMD;
    }else if (strcmp(cmd,"RNFR") == 0){
        cmd_e = RNFR;
    }else if (strcmp(cmd,"RNTO") == 0){
        cmd_e = RNTO;
    }else{
        cmd_e = OTHER;
    }

    return cmd_e;
}


int user_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    if (m_client->state == NOTLOGGED)
    {
        if (strcmp(param,"anonymous") == 0){
            //正确情况
            m_client->state = NEEDPASS;
            response_len = make_response(331,"Anonymous user accepted. A PASS request needed.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_user_cmd(): %s(%d)\n", strerror(errno), errno);
				return 0;
            }
        }else{
            //client不是anonymous的
            response_len = make_response(530,"Username unacceptable.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_user_cmd(): %s(%d)\n", strerror(errno), errno);
				return 0;
            }
        }
    }else if (m_client->state == NEEDPASS){
        if (strcmp(param,"anonymous") == 0)
        {
            response_len = make_response(331,"A PASS request needed.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_user_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
        }else{
            m_client->state == NOTLOGGED;
            response_len = make_response(530,"Username unacceptable.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_user_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
        }
        
    }else if (m_client->state == LOGGED){
        response_len = make_response(230,"User already logged. User can't change.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_user_cmd(): %s(%d)\n", strerror(errno), errno);
			return 0;
        }
    }
    return 1;
}


int pass_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    if (m_client->state == NEEDPASS)
    {
        //看是否满足邮件格式
        regmatch_t pmatch;
        regex_t reg;
        //双斜杠进行转义
        // const char *pattern = "^[a-zA-Z0-9_-]+@[a-zA-Z0-9_-]+(\\.[a-zA-Z0-9_-]+)+$";
        const char *pattern = ".+@";
        regcomp(&reg, pattern, REG_EXTENDED);

        int status = regexec(&reg, param, 1, &pmatch, 0);
        if (status != 0)
        {
            //不满足邮件格式
            response_len = make_response(530,"Password should be email address.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_pass_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
        }else
        {
            //满足邮件格式
            m_client->state = LOGGED;
            response_len = make_response(230,"User logged in, proceed.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_pass_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
        }
    }else if (m_client->state == NOTLOGGED){
        //还没有用户名
        response_len = make_response(503,"Last command should be User.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_pass_cmd(): %s(%d)\n", strerror(errno), errno);
			return 0;
        }
    }else if (m_client->state == LOGGED){
        //已登录
        response_len = make_response(503,"User already logged.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_pass_cmd(): %s(%d)\n", strerror(errno), errno);
			return 0;
        }
    }
    return 1;
}


int port_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    char ip_addr[32] = {0};
    int port;
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_port_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    int h1,h2,h3,h4,p1,p2;
    int num_match;
    num_match = sscanf(param,"%d,%d,%d,%d,%d,%d",&h1,&h2,&h3,&h4,&p1,&p2);
    if (num_match != 6){
        //port 参数格式不对
        response_len = make_response(501,"Wrong paramater format.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_port_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    if (h1<0 || h1>255 || h2<0 || h2>255 || h3<0 || h3>255 || h4<0 || h4>255
    || p1<0 || p1>255 || p2<0 || p2>255){
        response_len = make_response(501,"Wrong paramater format.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_port_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    //已经有的socket要断开
    if (m_client->pasv_listening_fd != -1){
        close(m_client->pasv_listening_fd);
        m_client->pasv_listening_fd = -1;
    }
    
    if (m_client->data_fd != -1){
        close(m_client->data_fd);
        m_client->data_fd = -1;
    }

    
    //在client中保存一下ip和port
    memset(m_client->ip_addr, 0, sizeof(m_client->ip_addr));
    sprintf(ip_addr, "%d.%d.%d.%d", h1, h2, h3, h4);
    strcpy(m_client->ip_addr, ip_addr);
    
    m_client->port_trans = 256*p1 + p2;
    m_client->trans_model = PORTMODEL;
    
    response_len = make_response(200,"PORT command OK.", resp);
    num_w = write(m_client->commandfd, resp, response_len);
    if (num_w < 0)
    {
        printf("Error write_port_cmd(): %s(%d)\n", strerror(errno), errno);
        return 0;
    }
    return 1;
}


int pasv_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_pasv_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    
    if ((int)strlen(param) > 0){
        //不能携带参数
        response_len = make_response(501,"Parameter is prohibited.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_pasv_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    
    //已有的监听socket和传输socket要断开
    if (m_client->data_fd != -1){
        close(m_client->data_fd);
        m_client->data_fd = -1;
    }

    if (m_client->pasv_listening_fd != -1){
        close(m_client->pasv_listening_fd);
        m_client->pasv_listening_fd = -1;
    }
    
    
    int h1, h2, h3, h4, p1, p2;
    int num_match = sscanf(my_server_ip,"%d.%d.%d.%d", &h1, &h2, &h3, &h4);
    if (num_match != 4){
         //解析出错
        response_len = make_response(500,"Internal parsing error in pasv.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_pasv_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    
    int port;
    srand(time(0));
    port = rand()%45535 + 20000;
    p1 = port / 256;
    p2 = port % 256;
    char response_msg[32] = {0};
    sprintf(response_msg,"=%d,%d,%d,%d,%d,%d", h1, h2, h3, h4, p1, p2);

    
    //创建socket
    if ((m_client->pasv_listening_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        printf("Error socket() in pasv: %s(%d)\n", strerror(errno), errno);
		return 0;
    }
    
    //设置本机的ip和port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //绑定socket与本机端口
    if (bind(m_client->pasv_listening_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        response_len = make_response(110, "bind fail", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        printf("Error bind() in pasv: %s(%d)\n sock_fd: %d", strerror(errno), errno,m_client->pasv_listening_fd);
		return 0;
    }

    //监听socket
    if (listen(m_client->pasv_listening_fd, 10) == -1){
        response_len = make_response(110, "listen fail", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        printf("Error listen() in pasv: %s(%d)\n", strerror(errno), errno);
		return 0;
    }

    m_client->trans_model = PASVMODEL;
    response_len = make_response(227, response_msg, resp);
    num_w = write(m_client->commandfd, resp, response_len);
    if (num_w < 0)
    {
        response_len = make_response(110, "write fail", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        printf("Error write_pasv_cmd(): %s(%d)\n", strerror(errno), errno);
        return 0;
    }
    return 1;
}
//返回-1表示失败，否则返回新建的socket值
int port_connect(struct client* m_client){
    int sock_fd;
    struct sockaddr_in addr;
    if((sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        printf("Error socket() in port_connect: %s(%d)\n", strerror(errno), errno);
        return -1;    
    }
    m_client->data_fd = sock_fd;
    //设置客户端地址和端口
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_client->port_trans);
    if (inet_pton(AF_INET, m_client->ip_addr, &addr.sin_addr) <= 0){
        printf("Error inet_pton() in port_connect.\n");
        close(sock_fd);
        m_client->data_fd = -1;
        return -1;
    }

   if ((connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr))) < 0){
        printf("Error connect() in port_connect: %s(%d)\n", strerror(errno), errno);
        return -1;		    
    }
    m_client->data_fd = sock_fd;
    return sock_fd;
}

int pasv_connect(struct client* m_client){
    socklen_t client_len;
    int sock_fd;
    struct sockaddr_in client_addr;
    client_len = sizeof(client_addr);
    sock_fd = accept(m_client->pasv_listening_fd, (struct sockaddr *)&client_addr, &client_len);
    
    if (sock_fd < 0) return -1;
    //连接成功
    m_client->data_fd = sock_fd;
    return sock_fd;
}
 
//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int retr_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    
    //先准备打开文件，cd到client的name_prefix目录下
    chdir(m_client->name_prefix);
    FILE* file_p;
    if ((file_p = fopen(param, "r")) == NULL){
        response_len = make_response(550,"Can not find the file.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    fclose(file_p);
    


    if (m_client->trans_model != UNKNOWN)
    {
        //已经指定好传输模式了，先给client一个mark
        response_len = make_response(150,"Ready to retrieve file.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
    }else{
        //还未指定模式
        response_len = make_response(530,"USE PORT or PASV first.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    memset(resp, 0, sizeof(resp));
    
    if (m_client->data_fd == -1)
    {
        if (m_client->trans_model == PORTMODEL){
            //如果是port模式，需要server去建立通道

            int sock_fd = port_connect(m_client);
            if (sock_fd == -1)
            {
                response_len = make_response(425,"No TCP connection was established.", resp);
                num_w = write(m_client->commandfd, resp, response_len);
                if (num_w < 0){
                    printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                    return 0;
                }
                return 1;
            }

            //开始传输文件
            char file_buffer[BUF_SIZE] = {0};
            int file_block_len = 0;
            int file_ptr = open(param, O_RDONLY);//只读
            while (1){
                memset(file_buffer, 0, sizeof(file_buffer));
                file_block_len = read(file_ptr, file_buffer, BUF_SIZE);
                if (file_block_len == 0){
                    //到了文件尾
                    break;
                }else if(file_block_len < 0){
                    response_len = make_response(451,"Reading the file failed.", resp);
                    num_w = write(m_client->commandfd, resp, response_len);
                    if (num_w < 0){
                        printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                        return 0;
                    }
                    return 1;
                }else{
                    num_w = write(sock_fd, file_buffer, file_block_len);
                    if (num_w < 0){
                        printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                        //出错记得关闭文件和通道
                        close(file_ptr);
                        response_len = make_response(426,"The connection is broken.", resp);
                        int num_w2 = write(m_client->commandfd, resp, response_len);
                        if (num_w2 < 0){
                            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                            return 0;
                        }
                        close(sock_fd);
                        m_client->data_fd = -1;
                        return 1;
                    } 
                }
            }
            //文件传输完毕
            memset(resp,0,sizeof(resp));
            response_len = make_response(226,"The entire file was written to server buffer.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0){
                printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                //出错记得关闭文件和通道
                close(file_ptr);
                close(sock_fd);
                m_client->data_fd = -1;
                return 0;
            }
            //结束后关闭文件
            close(file_ptr);
            //结束后关闭通道
            close(sock_fd);
            m_client->data_fd = -1;
        }else if (m_client->trans_model == PASVMODEL){
            //如果是pasv模式，需要监听client，等它连接
            int sock_fd = pasv_connect(m_client);
            if (sock_fd == -1)
            {
                response_len = make_response(425,"No TCP connection was established.", resp);
                num_w = write(m_client->commandfd, resp, response_len);
                if (num_w < 0){
                    printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                    return 0;
                }
                return 1;
            }
            //开始传输文件
            char file_buffer[BUF_SIZE] = {0};
            int file_block_len = 0;
            int file_ptr = open(param, O_RDONLY);
            while (1){
                memset(file_buffer, 0, sizeof(file_buffer));
                file_block_len = read(file_ptr, file_buffer, BUF_SIZE);
                if (file_block_len == 0){
                    //到了文件尾
                    break;
                }else if(file_block_len < 0){
                    response_len = make_response(451,"Reading the file failed.", resp);
                    num_w = write(m_client->commandfd, resp, response_len);
                    if (num_w < 0){
                        printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                        return 0;
                    }
                    return 1;
                }else{
                    num_w = write(sock_fd, file_buffer, file_block_len);
                    if (num_w < 0){
                        printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                        //出错记得关闭文件和通道
                        close(file_ptr);
                        response_len = make_response(426,"The connection is broken.", resp);
                        int num_w2 = write(m_client->commandfd, resp, response_len);
                        if (num_w2 < 0){
                            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                            return 0;
                        }
                        close(sock_fd);
                        m_client->data_fd = -1;
                        return 1;
                    } 
                }
            }
            //文件传输完毕
            memset(resp,0,sizeof(resp));
            response_len = make_response(226,"The entire file was written to server buffer.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0){
                printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                //出错记得关闭文件和通道
                close(file_ptr);
                close(sock_fd);
                m_client->data_fd = -1;
                return 0;
            }
            //结束后关闭文件
            close(file_ptr);
            //结束后关闭通道
            close(sock_fd);
            m_client->data_fd = -1;
        }else{
            //只有上述两种模式，不可能有别的模式
            printf("Error trans_model.\n");
            return 0;
        }
    }
    return 1;
}

int stor_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    chdir(m_client->name_prefix);
    FILE* file_p;
    // “a” ：以附加的方式打开只写文件。
    // 若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到尾。
    if ((file_p = fopen(param,"a")) == NULL){
        response_len = make_response(451,"File created or found fail.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    fclose(file_p);

     if (m_client->trans_model != UNKNOWN)
    {
        //已经指定好传输模式了，先给client一个mark
        response_len = make_response(150,"Ready to stor file.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
    }else{
        //还未指定模式
        response_len = make_response(530,"USE PORT or PASV first.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    memset(resp, 0, sizeof(resp));
    if (m_client->data_fd == -1)
    {
        if (m_client->trans_model == PORTMODEL){
        //如果是port模式，需要server去建立通道
        int sock_fd = port_connect(m_client);
        if (sock_fd == -1)
        {
            response_len = make_response(425,"No TCP connection was established.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0){
                printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
            return 1;
        }

        //开始传输文件
        char file_buffer[BUF_SIZE] = {0};
        int file_block_len = 0;
        int file_ptr = open(param, O_WRONLY);
        if (file_ptr == -1){
            response_len = make_response(451,"File created or found fail-2.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0){
                printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
            return 1;
        }
        while (1){
            memset(file_buffer, 0, sizeof(file_buffer));
            file_block_len = read(sock_fd, file_buffer, BUF_SIZE);
            if (file_block_len < 0){
                printf("Error read_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                //出错记得关闭文件和通道
                if(close(file_ptr) != 0){
                    printf("File: %s  cannot be closed. \n", param);
                }
                response_len = make_response(426,"Reading file failed.", resp);
                int num_w2 = write(m_client->commandfd, resp, response_len);
                if (num_w2 < 0){
                    printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                    return 0;
                }
                close(sock_fd);
                return 1;
            }else if (file_block_len == 0){
                //读完了
                break;
            }else{
                //写入文件中
                num_w = write(file_ptr, file_buffer, file_block_len);
                if (num_w < 0){
                    printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                    //出错记得关闭文件和通道
                    close(file_ptr);
                    response_len = make_response(451,"Writing file failed.", resp);
                    int num_w2 = write(m_client->commandfd, resp, response_len);
                    if (num_w2 < 0){
                        printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                        return 0;
                    }
                    close(sock_fd);
                    return 1;
                } 
            }
        }
        //文件传输完毕
        memset(resp,0,sizeof(resp));
        response_len = make_response(226,"STOR file success.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0){
            printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
            //出错记得关闭文件和通道
            close(file_ptr);
            close(sock_fd);
            return 0;
        }
        //结束后关闭文件
        close(file_ptr);
        //结束后关闭通道
        close(sock_fd);
        m_client->data_fd = -1;

        }else if (m_client->trans_model == PASVMODEL){
            //如果是pasv模式，需要监听client，等它连接
            socklen_t client_len;
            int sock_fd = pasv_connect(m_client);
            if (sock_fd == -1)
            {
                response_len = make_response(425,"No TCP connection was established.", resp);
                num_w = write(m_client->commandfd, resp, response_len);
                if (num_w < 0){
                    printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
                    return 0;
                }
                return 1;
            }

            //开始传输文件
            char file_buffer[BUF_SIZE] = {0};
            int file_block_len = 0;
            int file_ptr = open(param, O_WRONLY);
            if (file_ptr == -1){
                response_len = make_response(451,"File created or found fail.", resp);
                num_w = write(m_client->commandfd, resp, response_len);
                if (num_w < 0){
                    printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                    return 0;
                }
                return 1;
            }
            while (1){
                memset(file_buffer, 0, sizeof(file_buffer));
                file_block_len = read(sock_fd, file_buffer, BUF_SIZE);
                if (file_block_len < 0){
                    printf("Error read_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                    //出错记得关闭文件和通道
                    close(file_ptr);
                    response_len = make_response(426,"Reading file failed.", resp);
                    int num_w2 = write(m_client->commandfd, resp, response_len);
                    if (num_w2 < 0){
                        printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                        return 0;
                    }
                    close(sock_fd);
                    return 1;
                }else if (file_block_len == 0){
                    //读完了
                    break;
                }else{
                    //写入文件中
                    num_w = write(file_ptr, file_buffer,file_block_len);
                    if (num_w < 0){
                        printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                        //出错记得关闭文件和通道
                        close(file_ptr);
                        response_len = make_response(451,"Writing file failed.", resp);
                        int num_w2 = write(m_client->commandfd, resp, response_len);
                        if (num_w2 < 0){
                            printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                            return 0;
                        }
                        close(sock_fd);
                        return 1;
                    } 
                }
            }
            //文件传输完毕
            memset(resp,0,sizeof(resp));
            response_len = make_response(226,"STOR file success.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0){
                printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
                //出错记得关闭文件和通道
                close(file_ptr);
                close(sock_fd);
                return 0;
            }
            //结束后关闭文件
            close(file_ptr);
            //结束后关闭通道
            close(sock_fd);
            m_client->data_fd = -1;

        }else{
            //只有上述两种模式，不可能有别的模式
            printf("Error trans_model.\n");
            return 0;
        }
    }
    return 1;
}

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int type_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};

    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_type_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    if (strcmp(param,"I") == 0){
        response_len = make_response(200,"Type set to I.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_type_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }else{
        response_len = make_response(501,"Unaccepted parameter.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_type_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
}

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int quit_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    //quit也不用登录
    if (strlen(param) > 0){
        response_len = make_response(501,"Quit parameter is prohibited.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_quit_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }


    response_len = make_response(221,"Bye.", resp);
    num_w = write(m_client->commandfd, resp, response_len);
    if (num_w < 0)
    {
        printf("Error write_quit_cmd(): %s(%d)\n", strerror(errno), errno);
        return 0;
    }

    close(m_client->commandfd);
    m_client->commandfd = -1;
    
    if (m_client->pasv_listening_fd != -1){
        close(m_client->pasv_listening_fd);
        m_client->pasv_listening_fd = -1;
    }
    if (m_client->data_fd != -1){
        close(m_client->data_fd);
        m_client->data_fd = -1;
    }
    
    return 1; 
}


//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int pwd_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    char name_prefix[PATH_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_pwd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    strcpy(name_prefix,"\"");
    strcat(name_prefix,m_client->name_prefix);
    strcat(name_prefix,"\"");

    response_len = make_response(257,name_prefix, resp);
    num_w = write(m_client->commandfd, resp, response_len);
    if (num_w < 0)
    {
        printf("Error write_pwd_cmd(): %s(%d)\n", strerror(errno), errno);
        return 0;
    }
    return 1;
}


//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int cwd_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    char nameprefix_msg[PATH_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_cwd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    //返回值执行成功则返回0，失败返回-1
    if (chdir(param) < 0)
    { //失败
        strcpy(nameprefix_msg, param);
        strcat(nameprefix_msg, ": No such file or directory.");
        response_len = make_response(550, nameprefix_msg, resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_cwd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }else{ //成功
        getcwd(nameprefix_msg, PATH_SIZE); //获得当前目录的绝对路径
        bzero(m_client->name_prefix, PATH_SIZE);
        strcpy(m_client->name_prefix, nameprefix_msg);
        response_len = make_response(250, "Okay", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_cwd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
}

//返回0：write错误  返回1：正确处理了此命令并对client产生了response
int mkd_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    char msg[BUF_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_mkd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    chdir(m_client->name_prefix);

    if (mkdir(param, 777) < 0){
        //7表示可读可写可执行
        //三个7分别对应所有者、用户组、其他用户
        //失败
        strcpy(msg, "\"");
        strcat(msg, param);
        strcat(msg, "\" directory creating failed.");
        response_len = make_response(550, msg, resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_mkd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }else{ //成功
        strcpy(msg, "\"");
        strcat(msg, param);
        strcat(msg, "\" directory created.");
        response_len = make_response(257, msg, resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_mkd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
}

//返回 0 表示失败， 1 表示成功
int del_dir(char *param)
{   //调用此函数时，是在client的name_prefix目录下
    //param 是 encoded path，即可能是绝对路径，可能是相对路径
    printf("1%s\n",param);
    DIR* dp = NULL;
    struct dirent* dirp;
    struct stat dir_stat;
    char pathname[BUF_SIZE] = {0};
    if(stat(param, &dir_stat) == -1) return 0;
    //是常规文件的话,就删除之
    if (S_ISREG(dir_stat.st_mode)){
        remove(param);
    } 
    else if (S_ISDIR(dir_stat.st_mode)){
        //是目录的话
        dp = opendir(param);
        if (dp == NULL) return 0;
        while((dirp = readdir(dp)) != NULL)
        {
            //会读到 当前目录. 和父目录.. 跳过它们
            printf("2%s\n",dirp->d_name);
            if ((strcmp(dirp->d_name,".") == 0) || (strcmp(dirp->d_name,"..") == 0) ){
                continue;
            }
            memset(pathname,0,sizeof(pathname));
            sprintf(pathname,"%s/%s", param, dirp->d_name);
            printf("7\n");
            del_dir(pathname);
            printf("8\n");
        }
        closedir(dp);
        rmdir(param);
    }else{
        return 0;
    }

    return 1;
}

int rmd_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rmd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    chdir(m_client->name_prefix);
    if (del_dir(param) == 0)
    {
        //删除失败
        response_len = make_response(550,"Removal failed.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rmd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }else{
         //删除成功
        response_len = make_response(250,"Removal succeeded.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rmd_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }



}


int rnfr_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        //用上条命令=OTHER使得表明rnfr没成功
        m_client->last_cmd = OTHER;
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rnfr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }
    chdir(m_client->name_prefix);
    if (access(param, F_OK) == 0)
    {   //F_OK表示只判断是否存在
        //文件存在
        memset(m_client->file_rnfr,0,sizeof(m_client->file_rnfr));
        strcpy(m_client->file_rnfr, param);
        response_len = make_response(350,"File exists.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rnfr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
    }else{
        //文件不存在
        //用上条命令=OTHER使得表明rnfr没成功
        m_client->last_cmd = OTHER;
        response_len = make_response(450,"File does not exist.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rnfr_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
    }
    return 1;
}


int rnto_cmd(char* param, struct client* m_client){
    int response_len;
    int num_w;
    char resp[BUF_SIZE] = {0};
    if (m_client->state != LOGGED)
    {   //未登录
        m_client->last_cmd = OTHER;
        response_len = make_response(530,"Not logged in.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rnto_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        return 1;
    }

    if (m_client->last_cmd != RNFR){
        response_len = make_response(503,"Last command should be RNFR.", resp);
        num_w = write(m_client->commandfd, resp, response_len);
        if (num_w < 0)
        {
            printf("Error write_rnto_cmd(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
    }else{
        chdir(m_client->name_prefix);
        if (rename(m_client->file_rnfr, param) == 0)
        {//成功
            response_len = make_response(250,"File was renamed.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_rnto_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
        }else{
            response_len = make_response(550,"Renaming file failed.", resp);
            num_w = write(m_client->commandfd, resp, response_len);
            if (num_w < 0)
            {
                printf("Error write_rnto_cmd(): %s(%d)\n", strerror(errno), errno);
                return 0;
            }
        }
    }
    return 1;
}






