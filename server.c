#include <sys/socket.h>

#include "utility.h"


int main(int argc, char **argv) {

	int port = SERVER_PORT;
	//全部初始化为‘\0’
	char root[PATH_SIZE] = {0}; 
	//得到命令行指定的port和root，若无则默认port为21，root为/tmp
	parse_argument(&port, root, argc, argv);

	//监听socket和连接socket不一样，后者用于数据传输
	int listenfd, connfd;		
	struct sockaddr_in server_addr;
	char commandline[BUF_SIZE] = {0};
	char response[BUF_SIZE] = {0};
	char param[BUF_SIZE] = {0};
	

	//创建监听socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//设置本机的ip和port
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//监听"0.0.0.0"

	//将本机的ip和port与socket绑定
	if (bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//开始监听socket
	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//持续监听连接请求
	while (1) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		//等待client的连接 -- 阻塞函数
		//用户的IP经过accept()后会存储在client_addr里
		if ((connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;//继续监听下一个
		}
		
		printf("get new client [%s:%d]\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
		//有新用户，所以开始用新进程
		pid_t id = fork();
		if (id < 0)
		{
			printf("Error fork()\n");
			close(connfd);
			return 1;
		}
		else if (id == 0)
		{
			//是子进程
			//子进程继承了父进程的文件描述符表，但子进程不需要监听，只需要传命令
			close(listenfd);
			memset(response,0,sizeof(response));
			int response_len = make_response(220, "Anonymous FTP server ready.", response);
			int num_write = write(connfd, response, response_len);
			if (num_write < 0) {
					printf("Error write_welcome(): %s(%d)\n", strerror(errno), errno);
					return 1;
			}

			struct client new_client;
			initialize_client(&new_client, root);
			new_client.commandfd = connfd;

			while(1){
				//获得客户的命令
				memset(response,0,sizeof(response));
				memset(commandline,0,sizeof(commandline));
				int num_read = read(connfd, commandline, BUF_SIZE);
				if (num_read < 0) {
					printf("Error read()-2 command: %s(%d)\n", strerror(errno), errno);
					close(connfd);
					break;
				}
				//解析命令
				memset(param, 0, sizeof(param));
				int cmd_code = parse_cmdline(commandline, param);
				int k;
				switch (cmd_code)
				{
					case USER:
						if (user_cmd(param, &new_client) == 0)
						{ //有错误情况
							response_len = make_response(530,"USER cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_user_cmd(): %s(%d)\n", strerror(errno), errno);
								return 0;
							}
						}
						new_client.last_cmd = USER;
						break;
					case PASS:
						if (pass_cmd(param, &new_client) == 0)
						{ //有错误情况
							response_len = make_response(530,"PASS cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_pass_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = PASS;
						break;
					case RETR:
						if (retr_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"RETR cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_retr_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = RETR;
						break;
					case STOR:
						if (stor_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"STOR cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_stor_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = STOR;
						break;
					case QUIT:
						if (quit_cmd(param, &new_client) == 0)
						{   //有错误情况，connfd还没关
							response_len = make_response(530,"QUIT cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_quit_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}else{
							connfd = -1;
						}
						new_client.last_cmd = QUIT;
						break;
					case SYST:
						//SYST命令可以在用户登录前使用
						response_len = make_response(215,"UNIX Type: L8", response);
						num_write = write(connfd, response, response_len);
						if (num_write < 0)
						{
							printf("Error write_syst_cmd(): %s(%d)\n", strerror(errno), errno);
							return 1;
						}
						new_client.last_cmd = SYST;
						break;
					case TYPE:
						if (type_cmd(param,&new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"TYPE cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_type_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = TYPE;
						break;
					case PORT:
						if (port_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"PORT cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_port_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = PORT;
						break;
					case PASV:
						k = pasv_cmd(param, &new_client);
						if (k != 1)
						{   //有错误情况
							response_len = make_response(530,"PASV cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_pasv_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = PASV;
						break;
					case MKD:
						if (mkd_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"CWD cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_mkd_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = MKD;
						break;
					case CWD:
						if (cwd_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"CWD cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_cwd_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = CWD;
						break;
					case PWD:
						new_client.last_cmd = PWD;
						if (pwd_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"PWD cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_pwd_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						break;
					case LIST:
						new_client.last_cmd = LIST;
						break;
					case RMD:
						if (rmd_cmd(param, &new_client) == 0)
						{   
							response_len = make_response(530,"RMD cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_rmd_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = RMD;
						break;
					case RNFR:
						if (rnfr_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"RNFR cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_rnfr_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = RNFR;
						break;
					case RNTO:
						if (rnto_cmd(param, &new_client) == 0)
						{   //有错误情况
							response_len = make_response(530,"RNTO cmd error.", response);
							num_write = write(connfd, response, response_len);
							if (num_write < 0)
							{
								printf("Error write_rnto_cmd(): %s(%d)\n", strerror(errno), errno);
								return 1;
							}
						}
						new_client.last_cmd = RNTO;
						break;
					case OTHER:
						
						response_len = make_response(502, "Command not implemented.", response);
						num_write = write(connfd, response, response_len);
						new_client.last_cmd = OTHER;
						break;
				}
				if (connfd == -1) break;//客户可能输入了quit语句，使得connfd为-1了
	
			}
			//原来的子进程直接关闭socket，退出
			
			if (connfd != -1) close(connfd);
			exit(1);	
		}
		else{
			//是最开始的进程
			close(connfd);//不需要传输命令，只需要监听
			continue;
		}
	}

	close(listenfd);
	return 0;

}
