#include<stdio.h>
#include<stdlib.h>
#include<winsock2.h>
#include<string.h>
#include<cstdlib>
#include <Ws2tcpip.h>
#include<time.h>
#pragma comment(lib,"ws2_32.lib")
#define INIT_PORT 69       //��ʼĿ��˿ں�Ϊ69
#define DATA_LENGTH 512   //�������ݰ���data���ֵ���󳤶�
#define ACK_LENGTH 4     //����ack���ĳ���
#define RECEIVED_PACKET_LENGTH 1024  //����洢�յ��İ��Ļ���������
#define BUF_LENGTH 1024
/*���г�ʼ��*/
void startup() {
	WSADATA wsaData;
	int nRc = WSAStartup(0x0202, &wsaData);
	if (nRc)
	{
		//Winsock ��ʼ������
		exit(0);
	}
	if (wsaData.wVersion != 0x0202)
	{
		//�汾֧�ֲ���
		//���������û������ Winsock������
		WSACleanup();
		exit(0);
	}
}
/*��ʼ����ַ ��ڲ���ΪĿ��ip��˿�*/
SOCKADDR_IN addr_set(char*ip, short port){
	

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	
	return addr;
}
/*����UDP�׽���*/
SOCKET socket_init() {
	SOCKET clientSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);    //����UDP�׽���
	if (clientSock == INVALID_SOCKET)
	{
		printf("�����׽���ʧ��");
		exit(-1);
	}
	return clientSock;
}
/*����RQ��*/
char* RQ(char* filename,short operation_code,int transfer_mode,int* packet_length) {
	int name_length = strlen(filename);  //��¼�ļ�������
	//����һ��ռ䴢�����ݰ�
	*packet_length = 2 + name_length + 1 + transfer_mode + 1;  //��ͷ2�ֽ�Ϊ�����룬�м�Ϊ�ļ������ȣ�1��'\0',����ģʽ��1��'\0'
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
	memcpy(packet + 2, filename, name_length);   //д���ļ���
	packet[2 + name_length] = '\0';   //д��'\0'
	//д��transfer mode
	if (transfer_mode == 8)  
		memcpy(packet + 2 + name_length + 1, "netascii", 8);
	else if (transfer_mode == 5)
		memcpy(packet + 2 + name_length + 1, "octet", 5);
	packet[2 + name_length + 1 + transfer_mode] = '\0';

	
	return packet;
}
/*����DATA�� ��ڲ���Ϊ�ļ�ָ�룬���ݿ���*/
char* data(FILE* file,short block_num,int* data_length) {
	char data_part[DATA_LENGTH];  //���ڴ洢data_part
	int data_part_length = fread(data_part, 1, DATA_LENGTH, file);  //��ȡ�ļ�512���ֽڣ�����data_part
	if (ferror(file))
		return NULL;
	char* packet = (char*)malloc(2 + 2 + data_part_length);  //2���ֽڳ��Ĳ����룬2���ֽڳ������ݿ��ţ�data���ֳ���
	if (packet == NULL)
		return NULL;
	packet[0] = 0x00;
	packet[1] = 0x03;    //д�������
	block_num = htons(block_num);
	memcpy(packet + 2, &block_num, 2);  //д�����ݿ���
	memcpy(packet + 4, data_part, data_part_length); //д�����ݲ���
	*data_length = data_part_length + 4;   //��¼data������
	
	return packet;
}
/*����ACK�� ��ڲ���Ϊ���յ������ݿ���*/
char* ack(short block_num) {
	char* packet = (char*)malloc(ACK_LENGTH);
	if (packet == NULL)
		return NULL;
	packet[0] = 0x00;    //д�������
	packet[1] = 0x04;
	block_num = htons(block_num);  //ת��
	memcpy(packet+2,&block_num,2);   //д�����ݿ���
	
	return packet;
}
/*����error��Ϣ ��ڲ���Ϊ������*/
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
		return NULL;   //δ֪�Ĵ�����
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
	//������־
	FILE* log = fopen("TFTP_Clinet.log", "a");
	if (log == NULL) 
	{
		printf("��־�ļ���ʧ��\n");
		return -1;
	}
	setbuf(log, NULL);
		while (TRUE) {
		startup();   //��ʼ��
		//����ip
		/*char ip[20];
		printf("������ip��ַ:");
		scanf_s("%d", &ip);*/
		char ip[] = "127.0.0.1";

		SOCKADDR_IN addr;  //��ַ
		int addr_length = sizeof(addr);
		SOCKET ClientSock = socket_init();  //����udp�׽���

		//���ó�ʱ
		int snd_timeout = 1000;  //��ʱʱ��Ϊ1000ms
		int rcv_timeout = 1000;
		setsockopt(ClientSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&snd_timeout, sizeof(int));
		setsockopt(ClientSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcv_timeout, sizeof(int));

		//�����ش��������
		int resend_times = 0;
		int rerecv_times = 0;
		//�������ʱ��ֵ
		clock_t start, end;
		double runtime;
		double rate;

		short operation_code;
		
		                                                                                                                                                                                               
                                                                                                                                                                                              
		printf("0.�رտͻ���\n");
		printf("1.�����ļ�\n");
		printf("2.�ϴ��ļ�\n");
		printf("��ѡ�����:");
		scanf_s("%hd", &operation_code);
		if (operation_code == 0)   //�رտͻ���
		{
			printf("�ͻ��˹رճɹ�\n",40);
			break;
		}

		if (operation_code == 1)  //ѡ�����ط���
		{
			//�û�����
			printf("�������ļ�����");
			char download_filename[100];  //����ļ���
			scanf_s("%s", download_filename, 100);
			printf("1.netascii\n");
			printf("2.octet\n");
			printf("�����봫��ģʽ��");
			int transfer_mode;
			scanf_s("%d", &transfer_mode);
			if (transfer_mode == 1)
				transfer_mode = 8;
			else if (transfer_mode == 2)
				transfer_mode = 5;
			else
			{
				printf("��������ȷ�Ĵ���ģʽ\n");
				continue;
			}
			char temp_buf[BUF_LENGTH];   //���û��������ڴ洢�ѷ�����δȷ�ϵİ�
			int packet_length;    //�洢���ݳ���
			addr = addr_set(ip, INIT_PORT);   //�趨��ʼ��ַ
			char* RRQ = RQ(download_filename, operation_code, transfer_mode, &packet_length);
			memcpy(temp_buf, RRQ, BUF_LENGTH);   //���뷢����������
			free(RRQ);  //RRQû���ˣ��ͷſռ�
			int send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, sizeof(addr));   //����RRQ
			write_time(log);
			fprintf(log, "���������ļ�: %s\n", download_filename);
			start = clock();  //��ʱ��ʼ
			if (send_result == -1)  //����ʧ��
			{
				write_time(log);
				fprintf(log, "����ʧ��,�����ش�\n");

				resend_times = 0;
				
				while (resend_times <= 10)
				{
					send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, sizeof(addr));
					resend_times++;  //�ش�������1
					write_time(log);
					fprintf(log, "��%d���ش�\n", resend_times);
					if (send_result != -1)  //����ش��ɹ�
					{
						write_time(log);
						fprintf(log, "�ش��ɹ�\n");
						break;   //����ѭ��
					}
				}
			}

			if (resend_times > 10)  //���ʹ�������10����ʧ��
			{
				write_time(log);
				fprintf(log,"��������ʧ��\n");
				continue;  //��������
			}
			if (send_result == packet_length) {
				//û�����������
				
				//׼���ļ�
				FILE* download_file = fopen(download_filename, "wb");  //�ڱ��ؿ�һ���ļ�����д���յ�������
				if (download_file == NULL)
				{
					//�ļ�����ʧ�� ����
					write_time(log);
					fprintf(log,"�ļ�\"%s\"����ʧ��\n",download_file);
					continue;
				}
				//�ļ������ɹ�����ʼ���շ��ص�����

				SOCKADDR_IN server_addr;    //���ڴ�ŷ���˵ĵ�ַ���˿�
				int server_addr_length = sizeof(server_addr);   //����Ҫ��ָ�봫�س��ȣ���Ҫ����һ�����ȱ���

				short opcode;   //���ڴ洢��������	
				short block_num = 0; //���ڴ洢����block num
				short expect_num = 1; //���ڴ洢�����յ���block num����ֵΪ1
				short error_code;

				int fullsize = 0;  //���ڼ�¼�����ļ��Ĵ�С
				
				while (TRUE)
				{
					char received_packet[RECEIVED_PACKET_LENGTH];   //����յ��İ�
					memset(received_packet, 0, RECEIVED_PACKET_LENGTH);  //��ʼ��
					//���ձ���,rece_byte_num���ڼ�¼�յ����ֽ���
					int rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);  //�յ��İ������received_packet�У�
					
					rerecv_times = 0;
					if (rece_byte_num == -1)  //�հ�ʧ�� ,�����ش�
					{
						write_time(log);
						fprintf(log, "���ձ���ʧ��,�����ش�\n");
						while (rerecv_times <= 10)
						{
							//�ش�
							send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
							//ȷ���ش��ɹ�
							if (send_result == -1)  //�ش�ʧ��
							{
								resend_times = 0;
								while (resend_times <= 10)
								{
									send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);  //�ش�
									resend_times++;  //������1
									if (send_result != -1)  //�ش��ɹ�
									{
										break;
									}

								}
								if (resend_times > 10)  //����ʧ��
								{
									write_time(log);
									fprintf(log, "�ش�ʧ��\n");
									printf("����ʧ��\n");
									continue;
								}

							}
							//����
							rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);
							rerecv_times++;  //���մ�������
							if (rece_byte_num != -1)  //���ճɹ�
							{
								break;
							}
							
						}
						if (rerecv_times > 10)   //����ʧ��
						{
							write_time(log);
							fprintf(log,"���ձ���ʧ��\n");
							break;
							
						}
						
					}			

					//�жϰ�������
					memcpy(&opcode, received_packet, 2); //��ȡ�յ��İ���ǰ�����ֽڣ���������
					opcode = ntohs(opcode);
					if (opcode == 3)  //������Ϊ3��˵���յ��İ�Ϊdata��
					{
						addr = server_addr;  //���µ�ַ
						addr_length = server_addr_length; //���µ�ַ����
						memcpy(&block_num, received_packet + 2, 2);  //��ȡdata����block num
						//�����Ƿ��������İ�����Ҫ��һ��ack����block num��Ϊblock_num
						block_num = ntohs(block_num);
						write_time(log);
						fprintf(log, "�յ�DATA %hd\n", block_num);
						char* ACK = ack(block_num);
						packet_length = ACK_LENGTH;  //���°�����
						memcpy(temp_buf, ACK, packet_length);  //���뻺����
						free(ACK);  //�ͷ�ACK
						send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
						write_time(log);
						fprintf(log, "����ACK %hd\n", block_num);
						if (send_result == -1)  //����ʧ��
						{
							write_time(log);
							fprintf(log, "����ʧ�ܣ������ش�\n");
							resend_times = 0;
							
							while (resend_times <= 10)
							{
								send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
								resend_times++;  //�ش�������1
								write_time(log);
								fprintf(log, "��%d���ش�\n",resend_times);
								if (send_result != -1)  //����ش��ɹ�
								{
									write_time(log);
									fprintf(log, "�ش��ɹ�\n");
									break;   //����ѭ��
								}
							}
						}

						if (resend_times > 10)  //���ʹ�������10����ʧ��
						{
							write_time(log);
							fprintf(log, "����ʧ��\n");
							printf("����ʧ��");
							continue;  //��������
						}
						//������ɺ�

						if (block_num == expect_num)   //���Ϊ��Ҫ�İ�����д���ļ���expect_num��1 �ж��Ƿ�Ϊ���һ����
						{

							fullsize = fullsize + rece_byte_num - 4; //�ۼ��յ����ֽ���
							//��data���ֵ��ֽ�д���ļ���
							fwrite(received_packet + 4, 1, rece_byte_num - 4, download_file);
							expect_num++;  //������block num��1
							if (rece_byte_num > 0 && rece_byte_num - 4 < DATA_LENGTH)  //���յ����ֽ���С��512ʱ���������һ����
							{
								//������ش���;
								end = clock();
								printf("�������\n");
								runtime = (double)(end - start) / CLOCKS_PER_SEC;
								rate = fullsize / runtime / 1000;
								printf("�ܺ�ʱ: %21f s\n", runtime);
								printf("ƽ����������Ϊ: %21f kb/s\n",rate);
								write_time(log);
								fprintf(log, "�����ļ�\"%s\"�ɹ�,�ܺ�ʱ: %21f s,ƽ����������Ϊ: %21f kb/s\n", download_filename, runtime, rate);
								break;
							}
						}
						//��������һ����
						continue;
					}
					else if (opcode == 5)   //����յ�����error��
					{
						//��ȡ������
						memcpy(&error_code, received_packet + 2, 2);
						//���д�����
						//��ȡ������Ϣ
						error_code = ntohs(error_code);
						char* message = error_message(error_code);
						//������ش���;
						printf("error:%s\n", message);
						write_time(log);
						fprintf(log, "����:%s\n", message);
						//������ֹ
						break;
					}
					
				}
				fclose(download_file);
			}
		}
		if (operation_code == 2)  //ѡ���ϴ�����
		{
			//�û�����
			printf("�������ļ�����");
			char upload_filename[100];  //����ļ���
			scanf_s("%s", upload_filename, 100);
			printf("1.netascii\n");
			printf("2.octet\n");
			printf("�����봫��ģʽ��");
			int transfer_mode;
			scanf_s("%d", &transfer_mode);
			if (transfer_mode == 1)
				transfer_mode = 8;
			else if (transfer_mode == 2)
				transfer_mode = 5;
			else
			{
				printf("��������ȷ�Ĵ���ģʽ\n");
				continue;
			}
			char temp_buf[BUF_LENGTH];   //���û��������ڴ洢�ѷ�����δȷ�ϵİ�
			int packet_length;    //�洢���ݳ���
			addr = addr_set(ip, INIT_PORT);   //�趨��ʼ��ַ
			addr_length = sizeof(addr);
			char* WRQ = RQ(upload_filename, operation_code, transfer_mode, &packet_length);
			memcpy(temp_buf, WRQ, BUF_LENGTH);   //���뷢����������
			free(WRQ);
			int send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);   //����WRQ
			write_time(log);
			fprintf(log, "�����ϴ��ļ� %s\n", upload_filename);
			start = clock();
			if (send_result == -1)  //����ʧ��
			{
				resend_times = 0;
				write_time(log);
				fprintf(log, "����ʧ��,�����ش�\n");
				while (resend_times <= 10)
				{
					send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
					resend_times++;  //�ش�������1
					write_time(log);
					fprintf(log, "��%d���ش�\n", resend_times);
					if (send_result != -1)  //����ش��ɹ�
					{
						write_time(log);
						fprintf(log, "�ش��ɹ�\n");
						break;   //����ѭ��
					}
				}
			}

			if (resend_times > 10)  //���ʹ�������10����ʧ��
			{
				write_time(log);
				fprintf(log, "�����ϴ�ʧ��\n");
				continue;  //��������
			}

				//û�����������
				//׼���ļ�
				FILE* upload_file = fopen(upload_filename, "rb");  //�ڱ��ش�Ҫ�ϴ����ļ�
				if (upload_file == NULL)
				{
					//�ļ�����ʧ�� ����
					write_time(log);
					fprintf(log,"�ļ�����ʧ��\n");
					printf("�ļ�����ʧ�ܣ�����δ�ҵ����ļ�\n");
					continue;
				}
				//�ļ������ɹ�����ʼ���շ��ص�����

				SOCKADDR_IN server_addr;    //���ڴ�ŷ���˵ĵ�ַ���˿�
				int server_addr_length = sizeof(server_addr);   //����Ҫ��ָ�봫�س��ȣ���Ҫ����һ�����ȱ���

				char received_packet[RECEIVED_PACKET_LENGTH];   //����յ��İ�
				memset(received_packet, 0, RECEIVED_PACKET_LENGTH);  //��ʼ��
				short opcode;   //���ڴ洢��������	
				short block_num = 0; //���ڴ洢block num
				short rece_block_num = 1;  //���ڴ洢�յ��İ���block num
				int data_packet_length = -1;  //�������ݰ��ĳ���
				short error_code;
				int fullsize = 0; //���ڼ�¼����ɹ����ֽ���
				while (TRUE)
				{
					//���ձ���,rece_byte_num���ڼ�¼�յ����ֽ���
					int rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);  //�յ��İ������received_packet�У�
					
					if (rece_byte_num == -1)  //�հ�ʧ��,������ش�
					{
						write_time(log);
						fprintf(log, "���ձ���ʧ��,�����ش�\n");
						rerecv_times = 0;
						while (rerecv_times <= 10)
						{
							//�ش�
							send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
							//ȷ���ش��ɹ�
							if (send_result == -1)  //�ش�ʧ��
							{
								
								resend_times = 0;
								while (resend_times <= 10)
								{
									send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);  //�ش�
									resend_times++;  //������1
									if (send_result != -1)  //�ش��ɹ�
									{
										write_time(log);
										fprintf(log, "�ش��ɹ�\n");
										break;
									}

								}
								if (resend_times > 10)  //����ʧ��
								{
									write_time(log);
									fprintf(log,"����ʧ��\n");
									printf("����ʧ��\n");
									continue;
								}

							}
							//����
							rece_byte_num = recvfrom(ClientSock, received_packet, RECEIVED_PACKET_LENGTH, 0, (sockaddr*)&server_addr, &server_addr_length);
							rerecv_times++;  //���մ�������
							if (rece_byte_num != -1)  //���ճɹ�
							{
								write_time(log);
								fprintf(log,"���ճɹ�\n");
								break;
							}

						}
						if (rerecv_times > 10)   //����ʧ��
						{
							write_time(log);
							fprintf(log,"����ʧ��\n");
							break;
						}

					}
					//  �հ��ɹ�
						//�жϰ�������
						memcpy(&opcode, received_packet, 2); //��ȡ�յ��İ���ǰ�����ֽڣ���������
						opcode = ntohs(opcode);
						if (opcode == 4)  //������Ϊ4��˵���յ��İ�Ϊack��
						{
							
							memcpy(&rece_block_num, received_packet + 2, 2);  //��ȡblock num
							rece_block_num = ntohs(rece_block_num);
							write_time(log);
							fprintf(log, "�յ�ACK %hd\n", rece_block_num);
							if (rece_block_num == block_num)   //���Ϊ������ack��
							{
								addr = server_addr;  //���µ�ַ
								addr_length = server_addr_length;   //���³���
								if (data_packet_length > 0 && data_packet_length - 4 < DATA_LENGTH)  //���յ����ֽ���С��512ʱ���������һ����
								{
									end = clock();
									runtime = (double)(end - start) / CLOCKS_PER_SEC;
									rate = fullsize / runtime / 1000;
									
									printf("�ϴ����\n");
									printf("�ܺ�ʱ: %21f s\n", runtime);
									printf("ƽ����������: %21f kb/s\n",rate);
									write_time(log);
									fprintf(log, "�ϴ��ļ�\"%s\"�ɹ�,�ܺ�ʱ: %21f s,ƽ����������Ϊ: %21f kb/s\n", upload_filename, runtime, rate);
									break;
								}
								//������data��
								block_num++;
								char* DATA = data(upload_file,block_num,&data_packet_length);
								memcpy(temp_buf, DATA, data_packet_length);
								free(DATA);
								packet_length = data_packet_length;
								send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
								write_time(log);
								fprintf(log, "����DATA %hd\n", block_num);
								if (send_result == -1)  //����ʧ��
								{
									resend_times = 0;
									write_time(log);
									fprintf(log, "����ʧ��,�����ش�\n");
									while (resend_times <= 10)
									{
										send_result = sendto(ClientSock, temp_buf, packet_length, 0, (sockaddr*)&addr, addr_length);
										resend_times++;  //�ش�������1
										write_time(log);
										fprintf(log, "��%d���ش�\n", resend_times);
										if (send_result != -1)  //����ش��ɹ�
										{
											write_time(log);
											fprintf(log, "�ش��ɹ�\n");
											break;   //����ѭ��
										}
									}
								}

								if (resend_times > 10)  //���ʹ�������10����ʧ��
								{
									write_time(log);
									fprintf(log,"����ʧ��\n");
									break;  //��������
								}
								//����ɹ�
								fullsize = fullsize + data_packet_length - 4;
							}
						}
						else if (opcode == 5)   //����յ�����error��
						{
							//��ȡ������
							memcpy(&error_code, received_packet + 2, 2);
							//���д�����
							//��ȡ������Ϣ
							char* message = error_message(error_code);
							printf("error:%s\n", message);
							//������ֹ
							break;
						}
					
				}
				fclose(upload_file);
			
		}
	}


	return 0;
}