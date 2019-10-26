#include <sys/socket.h>

#include "utility.h"


int main(int argc, char **argv) {

	int port = SERVER_PORT;
	//ȫ����ʼ��Ϊ��\0��
	char root[PATH_SIZE] = {0}; 
	//�õ�������ָ����port��root��������Ĭ��portΪ21��rootΪ/tmp
	parse_argument(&port, root, argc, argv);

	//����socket������socket��һ���������������ݴ���
	int listenfd, connfd;		
	struct sockaddr_in server_addr;
	char commandline[BUF_SIZE] = {0};
	char response[BUF_SIZE] = {0};
	char param[BUF_SIZE] = {0};
	

	//��������socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//���ñ�����ip��port
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//����"0.0.0.0"

	//��������ip��port��socket��
	if (bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//��ʼ����socket
	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//����������������
	while (1) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		//�ȴ�client������ -- ��������
		//�û���IP����accept()���洢��client_addr��
		if ((connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;//����������һ��
		}
		
		printf("get new client [%s:%d]\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
		//�����û������Կ�ʼ���½���
		pid_t id = fork();
		if (id < 0)
		{
			printf("Error fork()\n");
			close(connfd);
			return 1;
		}
		else if (id == 0)
		{
			//���ӽ���
			//�ӽ��̼̳��˸����̵��ļ������������ӽ��̲���Ҫ������ֻ��Ҫ������
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
				//��ÿͻ�������
				memset(response,0,sizeof(response));
				memset(commandline,0,sizeof(commandline));
				int num_read = read(connfd, commandline, BUF_SIZE);
				if (num_read < 0) {
					printf("Error read()-2 command: %s(%d)\n", strerror(errno), errno);
					close(connfd);
					break;
				}
				//��������
				memset(param, 0, sizeof(param));
				int cmd_code = parse_cmdline(commandline, param);
				int k;
				switch (cmd_code)
				{
					case USER:
						if (user_cmd(param, &new_client) == 0)
						{ //�д������
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
						{ //�д������
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
						{   //�д������
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
						{   //�д������
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
						{   //�д��������connfd��û��
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
						//SYST����������û���¼ǰʹ��
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
						{   //�д������
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
						{   //�д������
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
						{   //�д������
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
						{   //�д������
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
						{   //�д������
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
						{   //�д������
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
						{   //�д������
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
						{   //�д������
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
				if (connfd == -1) break;//�ͻ�����������quit��䣬ʹ��connfdΪ-1��
	
			}
			//ԭ�����ӽ���ֱ�ӹر�socket���˳�
			
			if (connfd != -1) close(connfd);
			exit(1);	
		}
		else{
			//���ʼ�Ľ���
			close(connfd);//����Ҫ�������ֻ��Ҫ����
			continue;
		}
	}

	close(listenfd);
	return 0;

}
