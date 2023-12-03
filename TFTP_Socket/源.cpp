#include<stdio.h>
#include<stdlib.h>
#include<winsock2.h>
#include<string.h>
#include<cstdlib>
#include <Ws2tcpip.h>
#include<time.h>
#pragma comment(lib,"ws2_32.lib")
#define INIT_PORT 69       //初始目标端口号为69
#define DATA_LENGTH 512   //定义数据包中data部分的最大长度
#define ACK_LENGTH 4     //定义ack包的长度
#define RECEIVED_PACKET_LENGTH 1024  //定义存储收到的包的缓冲区长度
#define BUF_LENGTH 1024
/*进行初始化*/
void startup() {
	WSADATA wsaData;
	int nRc = WSAStartup(0x0202, &wsaData);
	if (nRc)
	{
		//Winsock 初始化错误
		exit(0);
	}
	if (wsaData.wVersion != 0x0202)
	{
		//版本支持不够
		//报告错误给用户，清除 Winsock，返回
		WSACleanup();
		exit(0);
	}
}
/*初始化地址 入口参数为目标ip与端口*/
SOCKADDR_IN addr_set(char*ip, short port){
	

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	
	return addr;
}
/*设置UDP套接字*/
SOCKET socket_init() {
	SOCKET clientSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);    //创建UDP套接字
	if (clientSock == INVALID_SOCKET)
	{
		printf("创建套接字失败");
		exit(-1);
	}
	return clientSock;
}
/*生成RQ包*/
char* RQ(char* filename,short operation_code,int transfer_mode,int* packet_length) {
	int name_length = strlen(filename);  //记录文件名长度
	//申请一块空间储存数据包
	*packet_length = 2 + name_length + 1 + transfer_mode + 1;  //开头2字节为操作码，中间为文件名长度，1个'\0',传送模式，1个'\0'
	char* packet = (char*)malloc(*packet_length); 
	if (packet == NULL)
		return NULL;
	if (operation_code == 1)
	{
		packet[0] = 0x00;
		packet[1] = 0x01;
	}
	else if (operation_code == 2)
	{
		packet[0] = 0x00;
		packet[1] = 0x02;
	}
	memcpy(packet + 2, filename, name_length);   //写入文件名
	packet[2 + name_length] = '\0';   //写入'\0'
	//写入transfer mode
	if (transfer_mode == 8)  
		memcpy(packet + 2 + name_length + 1, "netascii", 8);
	else if (transfer_mode == 5)
		memcpy(packet + 2 + name_length + 1, "octet", 5);
	packet[2 + name_length + 1 + transfer_mode] = '\0';

	
	return packet;
}
/*生成DATA包 入口参数为文件指针，数据块编号*/
char* data(FILE* file,short block_num,int* data_length) {
	char data_part[DATA_LENGTH];  //用于存储data_part
	int data_part_length = fread(data_part, 1, DATA_LENGTH, file);  //读取文件512个字节，存于data_part
	if (ferror(file))
		return NULL;
	char* packet = (char*)malloc(2 + 2 + data_part_length);  //2个字节长的操作码，2个字节长的数据块编号，data部分长度
	if (packet == NULL)
		return NULL;
	packet[0] = 0x00;
	packet[1] = 0x03;    //写入操作码
	block_num = htons(block_num);
	memcpy(packet + 2, &block_num, 2);  //写入数据块编号
	memcpy(packet + 4, data_part, data_part_length); //写入数据部分
	*data_length = data_part_length + 4;   //记录data包长度
	
	return packet;
}
/*生成ACK包 入口参数为接收到的数据块编号*/
char* ack(short block_num) {
	char* packet = (char*)malloc(ACK_LENGTH);
	if (packet == NULL)
		return NULL;
	packet[0] = 0x00;    //写入操作码
	packet[1] = 0x04;
	block_num = htons(block_num);  //转换
	memcpy(packet+2,&block_num,2);   //写入数据块编号
	
	return packet;
}
/*生成error信息 入口参数为错误码*/
char* error_message(short error_code) {
	
	char *message = (char*)malloc(50);
	switch (error_code)
	{
	case 0:
		strcpy(message,"Not defined, see error message");
		break;
	case 1:
		strcpy(message,"File not found");
		break;
	case 2:
		strcpy(message,"Access violation");
		break;
	case 3:
		strcpy(message,"Disk full or allocation exceeded");
		break;
	case 4:
		strcpy(message,"Illegal TFTP operation");
		break;
	case 5:
		strcpy(message,"ID Unknown transfer ID");
		break;
	case 6:
		strcpy(message,"File already exists");
		break;
	case 7:
		strcpy(message,"No such user");
		break;
	default:
		return NULL;   //未知的错误码
		break;
	}

	return message;
}
void write_time(FILE* log)
{
	time_t current_time;
	time(&current_time);
	char stime[80];
	strcpy(stime, ctime(&current_time));
	*(strchr(stime, '\n')) = '\0';
	fprintf(log, "[ %s ]", stime);
	return;
}
int main()
{
	//开启日志
	FILE* log = fopen("TFTP_Clinet.log", "a");
	if (log == NULL) 
	{
		printf("日志文件打开失败\n");
		return -1;
	}
	setbuf(log, NULL);
		while (TRUE) {
		startup();   //初始化
		//输入ip
		/*char ip[20];
		printf("请输入ip地址:");
		scanf_s("%d", &ip);*/
		char ip[] = "127.0.0.1";

		SOCKADDR_IN addr;  //地址
		int addr_length = sizeof(addr);
		SOCKET ClientSock = socket_init();  //创建udp套接字

		//设置超时
		int snd_timeout = 1000;  //超时时长为1000ms
		int rcv_timeout = 1000;
		setsockopt(ClientSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&snd_timeout, sizeof(int));
		setsockopt(ClientSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcv_timeout, sizeof(int));

		//设置重传所需变量
		int resend_times = 0;
		int rerecv_times = 0;
		//设置相关时间值
		clock_t start, end;
		double runtime;
		double rate;

		short operation_code;
		
		                                                                                                                                                                                               
                                                                                                                                                                                              
		printf("0.关闭客户端\n");
		printf("1.下载文件\n");
		printf("2.上传文件\n");
		printf("请选择服务:");
		scanf_s("%hd", &operation_code);
		if (operation_code == 0)   //关闭客户端
		{
			printf("客户端关闭成功\n",40);
			break;
		}

		if (operation_code == 1)  //选择下载服务
		{
			//用户界面
			printf("请输入文件名：");
			char download_filename[100];  //存放文件名
			scanf_s("%s", download_filename, 100);
			printf("1.netascii\n");
			printf("2.octet\n");
			printf("请输入传输模式：");
			int transfer_mode;
			scanf_s("%d", &transfer_mode);
			if (transfer_mode == 1)
				transfer_mode = 8;
			else if (transfer_mode == 2)
				transfer_mode = 5;
			else
			{
				printf("请输入正确的传输模式\n");
				continue;
			}
			char temp_buf[BUF_LENGTH];   //设置缓冲区用于存储已发出但未确认的包
			int packet_length;    //存储数据长度
			addr = addr_set(ip, INIT_PORT);   //设定初始地址
			char* RRQ = RQ(download_filename, operation_code, transfer_mode, &packet_length);
			memcpy(temp_buf, RRQ, BUF_LENGTH);   //存入发包缓冲区中
			free(RRQ);  //RRQ没用了，释放空间
			int send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, sizeof(addr));   //发送RRQ
			write_time(log);
			fprintf(log, "请求下载文件: %s\n", download_filename);
			start = clock();  //计时开始
			if (send_result == -1)  //发送失败
			{
				write_time(log);
				fprintf(log, "请求失败,进行重传\n");

				resend_times = 0;
				
				while (resend_times <= 10)
				{
					send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, sizeof(addr));
					resend_times++;  //重传次数加1
					write_time(log);
					fprintf(log, "第%d次重传\n", resend_times);
					if (send_result != -1)  //如果重传成功
					{
						write_time(log);
						fprintf(log, "重传成功\n");
						break;   //跳出循环
					}
				}
			}

			if (resend_times > 10)  //传送次数大于10则传送失败
			{
				write_time(log);
				fprintf(log,"请求下载失败\n");
				continue;  //结束传输
			}
			if (send_result == packet_length) {
				//没错，则接收数据
				
				//准备文件
				FILE* download_file = fopen(download_filename, "wb");  //在本地开一个文件用于写入收到的数据
				if (download_file == NULL)
				{
					//文件开启失败 报错
					write_time(log);
					fprintf(log,"文件\"%s\"开启失败\n",download_file);
					continue;
				}
				//文件开启成功，开始接收返回的数据

				SOCKADDR_IN server_addr;    //用于存放服务端的地址及端口
				int server_addr_length = sizeof(server_addr);   //由于要用指针传回长度，需要定义一个长度变量

				short opcode;   //用于存储包的类型	
				short block_num = 0; //用于存储包的block num
				short expect_num = 1; //用于存储期望收到的block num，初值为1
				short error_code;

				int fullsize = 0;  //用于记录传输文件的大小
				
				while (TRUE)
				{
					char received_packet[RECEIVED_PACKET_LENGTH];   //存放收到的包
					memset(received_packet, 0, RECEIVED_PACKET_LENGTH);  //初始化
					//接收报文,rece_byte_num用于记录收到的字节数
					int rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);  //收到的包存放在received_packet中，
					
					rerecv_times = 0;
					if (rece_byte_num == -1)  //收包失败 ,进行重传
					{
						write_time(log);
						fprintf(log, "接收报文失败,进行重传\n");
						while (rerecv_times <= 10)
						{
							//重传
							send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
							//确保重传成功
							if (send_result == -1)  //重传失败
							{
								resend_times = 0;
								while (resend_times <= 10)
								{
									send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);  //重传
									resend_times++;  //次数加1
									if (send_result != -1)  //重传成功
									{
										break;
									}

								}
								if (resend_times > 10)  //传输失败
								{
									write_time(log);
									fprintf(log, "重传失败\n");
									printf("传输失败\n");
									continue;
								}

							}
							//重收
							rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);
							rerecv_times++;  //重收次数增加
							if (rece_byte_num != -1)  //接收成功
							{
								break;
							}
							
						}
						if (rerecv_times > 10)   //接收失败
						{
							write_time(log);
							fprintf(log,"接收报文失败\n");
							break;
							
						}
						
					}			

					//判断包的类型
					memcpy(&opcode, received_packet, 2); //读取收到的包的前两个字节，即操作码
					opcode = ntohs(opcode);
					if (opcode == 3)  //操作码为3，说明收到的包为data包
					{
						addr = server_addr;  //更新地址
						addr_length = server_addr_length; //更新地址长度
						memcpy(&block_num, received_packet + 2, 2);  //读取data包的block num
						//无论是否是期望的包，都要回一个ack，其block num就为block_num
						block_num = ntohs(block_num);
						write_time(log);
						fprintf(log, "收到DATA %hd\n", block_num);
						char* ACK = ack(block_num);
						packet_length = ACK_LENGTH;  //更新包长度
						memcpy(temp_buf, ACK, packet_length);  //存入缓冲区
						free(ACK);  //释放ACK
						send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
						write_time(log);
						fprintf(log, "发送ACK %hd\n", block_num);
						if (send_result == -1)  //发送失败
						{
							write_time(log);
							fprintf(log, "发送失败，进行重传\n");
							resend_times = 0;
							
							while (resend_times <= 10)
							{
								send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
								resend_times++;  //重传次数加1
								write_time(log);
								fprintf(log, "第%d次重传\n",resend_times);
								if (send_result != -1)  //如果重传成功
								{
									write_time(log);
									fprintf(log, "重传成功\n");
									break;   //跳出循环
								}
							}
						}

						if (resend_times > 10)  //传送次数大于10则传送失败
						{
							write_time(log);
							fprintf(log, "传输失败\n");
							printf("传输失败");
							continue;  //结束传输
						}
						//发送完成后

						if (block_num == expect_num)   //如果为需要的包，则写入文件，expect_num加1 判断是否为最后一个包
						{

							fullsize = fullsize + rece_byte_num - 4; //累加收到的字节数
							//将data部分的字节写入文件中
							fwrite(received_packet + 4, 1, rece_byte_num - 4, download_file);
							expect_num++;  //期望的block num加1
							if (rece_byte_num > 0 && rece_byte_num - 4 < DATA_LENGTH)  //当收到的字节数小于512时，这是最后一个包
							{
								//进行相关处理;
								end = clock();
								printf("传输完成\n");
								runtime = (double)(end - start) / CLOCKS_PER_SEC;
								rate = fullsize / runtime / 1000;
								printf("总耗时: %21f s\n", runtime);
								printf("平均传输速率为: %21f kb/s\n",rate);
								write_time(log);
								fprintf(log, "下载文件\"%s\"成功,总耗时: %21f s,平均传输速率为: %21f kb/s\n", download_filename, runtime, rate);
								break;
							}
						}
						//继续收下一个包
						continue;
					}
					else if (opcode == 5)   //如果收到的是error包
					{
						//读取错误码
						memcpy(&error_code, received_packet + 2, 2);
						//进行错误处理
						//获取错误信息
						error_code = ntohs(error_code);
						char* message = error_message(error_code);
						//进行相关处理;
						printf("error:%s\n", message);
						write_time(log);
						fprintf(log, "错误:%s\n", message);
						//接收终止
						break;
					}
					
				}
				fclose(download_file);
			}
		}
		if (operation_code == 2)  //选择上传服务
		{
			//用户界面
			printf("请输入文件名：");
			char upload_filename[100];  //存放文件名
			scanf_s("%s", upload_filename, 100);
			printf("1.netascii\n");
			printf("2.octet\n");
			printf("请输入传输模式：");
			int transfer_mode;
			scanf_s("%d", &transfer_mode);
			if (transfer_mode == 1)
				transfer_mode = 8;
			else if (transfer_mode == 2)
				transfer_mode = 5;
			else
			{
				printf("请输入正确的传输模式\n");
				continue;
			}
			char temp_buf[BUF_LENGTH];   //设置缓冲区用于存储已发出但未确认的包
			int packet_length;    //存储数据长度
			addr = addr_set(ip, INIT_PORT);   //设定初始地址
			addr_length = sizeof(addr);
			char* WRQ = RQ(upload_filename, operation_code, transfer_mode, &packet_length);
			memcpy(temp_buf, WRQ, BUF_LENGTH);   //存入发包缓冲区中
			free(WRQ);
			int send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);   //发送WRQ
			write_time(log);
			fprintf(log, "请求上传文件 %s\n", upload_filename);
			start = clock();
			if (send_result == -1)  //发送失败
			{
				resend_times = 0;
				write_time(log);
				fprintf(log, "请求失败,进行重传\n");
				while (resend_times <= 10)
				{
					send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
					resend_times++;  //重传次数加1
					write_time(log);
					fprintf(log, "第%d次重传\n", resend_times);
					if (send_result != -1)  //如果重传成功
					{
						write_time(log);
						fprintf(log, "重传成功\n");
						break;   //跳出循环
					}
				}
			}

			if (resend_times > 10)  //传送次数大于10则传送失败
			{
				write_time(log);
				fprintf(log, "请求上传失败\n");
				continue;  //结束传输
			}

				//没错，则接收数据
				//准备文件
				FILE* upload_file = fopen(upload_filename, "rb");  //在本地打开要上传的文件
				if (upload_file == NULL)
				{
					//文件开启失败 报错
					write_time(log);
					fprintf(log,"文件开启失败\n");
					printf("文件开启失败，可能未找到该文件\n");
					continue;
				}
				//文件开启成功，开始接收返回的数据

				SOCKADDR_IN server_addr;    //用于存放服务端的地址及端口
				int server_addr_length = sizeof(server_addr);   //由于要用指针传回长度，需要定义一个长度变量

				char received_packet[RECEIVED_PACKET_LENGTH];   //存放收到的包
				memset(received_packet, 0, RECEIVED_PACKET_LENGTH);  //初始化
				short opcode;   //用于存储包的类型	
				short block_num = 0; //用于存储block num
				short rece_block_num = 1;  //用于存储收到的包中block num
				int data_packet_length = -1;  //用于数据包的长度
				short error_code;
				int fullsize = 0; //用于记录传输成功的字节数
				while (TRUE)
				{
					//接收报文,rece_byte_num用于记录收到的字节数
					int rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);  //收到的包存放在received_packet中，
					
					if (rece_byte_num == -1)  //收包失败,则进行重传
					{
						write_time(log);
						fprintf(log, "接收报文失败,进行重传\n");
						rerecv_times = 0;
						while (rerecv_times <= 10)
						{
							//重传
							send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
							//确保重传成功
							if (send_result == -1)  //重传失败
							{
								
								resend_times = 0;
								while (resend_times <= 10)
								{
									send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);  //重传
									resend_times++;  //次数加1
									if (send_result != -1)  //重传成功
									{
										write_time(log);
										fprintf(log, "重传成功\n");
										break;
									}

								}
								if (resend_times > 10)  //传输失败
								{
									write_time(log);
									fprintf(log,"传输失败\n");
									printf("传输失败\n");
									continue;
								}

							}
							//重收
							rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);
							rerecv_times++;  //重收次数增加
							if (rece_byte_num != -1)  //接收成功
							{
								write_time(log);
								fprintf(log,"接收成功\n");
								break;
							}

						}
						if (rerecv_times > 10)   //接收失败
						{
							write_time(log);
							fprintf(log,"接收失败\n");
							break;
						}

					}
					//  收包成功
						//判断包的类型
						memcpy(&opcode, received_packet, 2); //读取收到的包的前两个字节，即操作码
						opcode = ntohs(opcode);
						if (opcode == 4)  //操作码为4，说明收到的包为ack包
						{
							
							memcpy(&rece_block_num, received_packet + 2, 2);  //读取block num
							rece_block_num = ntohs(rece_block_num);
							write_time(log);
							fprintf(log, "收到ACK %hd\n", rece_block_num);
							if (rece_block_num == block_num)   //如果为期望的ack包
							{
								addr = server_addr;  //更新地址
								addr_length = server_addr_length;   //更新长度
								if (data_packet_length > 0 && data_packet_length - 4 < DATA_LENGTH)  //当收到的字节数小于512时，这是最后一个包
								{
									end = clock();
									runtime = (double)(end - start) / CLOCKS_PER_SEC;
									rate = fullsize / runtime / 1000;
									
									printf("上传完成\n");
									printf("总耗时: %21f s\n", runtime);
									printf("平均传输速率: %21f kb/s\n",rate);
									write_time(log);
									fprintf(log, "上传文件\"%s\"成功,总耗时: %21f s,平均传输速率为: %21f kb/s\n", upload_filename, runtime, rate);
									break;
								}
								//否则发送data包
								block_num++;
								char* DATA = data(upload_file,block_num,&data_packet_length);
								memcpy(temp_buf, DATA, data_packet_length);
								free(DATA);
								packet_length = data_packet_length;
								send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
								write_time(log);
								fprintf(log, "发送DATA %hd\n", block_num);
								if (send_result == -1)  //发送失败
								{
									resend_times = 0;
									write_time(log);
									fprintf(log, "发送失败,进行重传\n");
									while (resend_times <= 10)
									{
										send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
										resend_times++;  //重传次数加1
										write_time(log);
										fprintf(log, "第%d次重传\n", resend_times);
										if (send_result != -1)  //如果重传成功
										{
											write_time(log);
											fprintf(log, "重传成功\n");
											break;   //跳出循环
										}
									}
								}

								if (resend_times > 10)  //传送次数大于10则传送失败
								{
									write_time(log);
									fprintf(log,"传送失败\n");
									break;  //结束传输
								}
								//传输成功
								fullsize = fullsize + data_packet_length - 4;
							}
						}
						else if (opcode == 5)   //如果收到的是error包
						{
							//读取错误码
							memcpy(&error_code, received_packet + 2, 2);
							//进行错误处理
							//获取错误信息
							char* message = error_message(error_code);
							printf("error:%s\n", message);
							//接收终止
							break;
						}
					
				}
				fclose(upload_file);
			
		}
	}


	return 0;
}