#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <mutex>

#include "Message.h"

// lab1���ᵽ����ֹ��������
#pragma comment (lib, "ws2_32.lib")
std::mutex mtx; // ��������Ļ�����

using namespace std;

// ����·�����ͷ������˵Ķ˿ں�
const int RouterPORT = 23381; //·�����˿ں�
const int ServerPORT = 23382; //�ͻ��˶˿ں�

// ���÷�����socket��·����socket
SOCKET Router_Socket;
SOCKET Server_Socket;

SOCKADDR_IN Router_Addr;
SOCKADDR_IN Server_Addr;

// ȫ�ֱ���
int recv_mark = -1;  //���ر��
int addr_len = sizeof(Router_Addr);   //��ַ����
//Ϊ�����֣����Ƿ����������кŴ�1000��ʼ
int now_seq = 1000;  //�������˵����к�
int now_ack = 0; //�������˵�ȷ�Ϻ�
int data_state = 0; //���ݴ���״̬

// �������������ȫ�ֱ���
int now_GBN = 0; //GBN���кŵ�ȷ��
int MaxWindowSize;  //���͵Ļ������ڴ�С


DWORD WINAPI ReceiveThread(LPVOID pParam) {
	DATA_PACKAGE* message3 = (DATA_PACKAGE*)pParam;
	recv_mark = recvfrom(Server_Socket, (char*)message3, sizeof(*message3), 0, (sockaddr*)&Router_Addr, &addr_len);
	return 0;
}

bool Close_TCP_Connect(DATA_PACKAGE now_message) {
	DATA_PACKAGE message1 = now_message;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;
	DATA_PACKAGE message4;

	//----------- �����һ�λ��ֵ���Ϣ��FIN=1��seq=u��-----------
	cout << "�������˵�һ�λ��ֳɹ��յ���";
	//�����λ
	if ((message1.Udp_Header.flag & FIN) && (message1.Udp_Header.Ack == now_seq)){
		if (!check(message1)) return false;
		cout << "����Ϣ׼ȷ����" << endl;
	}
	else return false;



	//----------- ���͵ڶ��λ��ֵ���Ϣ��ACK=1��seq =v ��ack=u+1��-----------
	message2.Udp_Header.SrcPort = ServerPORT;
	message2.Udp_Header.DestPort = RouterPORT;
	message2.Udp_Header.Ack = message1.Udp_Header.Seq+1;
	message2.Udp_Header.Seq = now_seq;
	message2.Udp_Header.flag += ACK;
	set_checkNum(message2);//����У���

	int send2 = sendto(Server_Socket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&Router_Addr, addr_len);

	//�����û�гɹ�����
	if (send2 == 0) return false;
	cout << "�������˵ڶ��λ��ֳɹ����ͣ�" << endl;

	//----------- �ȴ����ݴ������-----------
	while (1) {
		if (data_state == 0)break;
	}

	//----------- ���͵����λ��ֵ���Ϣ��FIN=1��ACK=1��seq=w��ack=u+1��-----------

	message3.Udp_Header.SrcPort = ServerPORT;
	message3.Udp_Header.DestPort = RouterPORT;
	message3.Udp_Header.Ack = message1.Udp_Header.Seq+1;
	message3.Udp_Header.Seq = now_seq;
	message3.Udp_Header.flag += ACK;
	message3.Udp_Header.flag += FIN;
	now_ack = message3.Udp_Header.Ack;
	set_checkNum(message3);//����У���

	int send3 = sendto(Server_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, addr_len);
	clock_t message3_start = clock();

	//�����û�гɹ�����
	if (send3 == 0) return false;
	cout << "�������˵����λ��ֳɹ����ͣ�" << endl;

	//�������̣߳����ڼ��������յ��ڶ������֣����߳����ڳ�ʱ�ش�����
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message4, 0, 0);

	//----------- ���յ��Ĵλ��ֵ���Ϣ��ACK=1��seq=u+1��ack = w+1��-----------
	now_seq++;
	//����ͳ�ʱ�ش�
	while (1) {
		//��Ϊ��1��ʱ������յ�����Ϣ
		if (recv_mark == 0) return false;
		//�ɹ��յ���Ϣ
		else if (recv_mark > 0) {
			//�����λ�����к�ack����֤�ɿ��Դ���
			cout << "�������˵��Ĵλ��ֳɹ��յ���";
			if ((message4.Udp_Header.flag & ACK) && (message4.Udp_Header.Ack == message3.Udp_Header.Seq + 1)) {
				if (!check(message4)) return false;
				cout << "����Ϣ׼ȷ����" << endl;
				break;
			}
			else return false;
		}
		//message3��ʱ��׼���ش�
		if (clock() - message3_start > MaxWaitTime)
		{
			cout << "�����λ��ֳ�ʱ��׼���ش�......" << endl;
			send3 = sendto(Server_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, addr_len);
			message3_start = clock();
			if (send3 == 0) return false;
		}
	}
	recv_mark = -1;

	cout << "�������˹ر����ӳɹ�����ʱTCP���кš�ȷ�Ϻ�Ϊ" << now_seq << " " << now_ack << endl;
}

bool TCP_Connect() {
	DATA_PACKAGE message1;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;


	//----------- ���յ�һ�����ֵ���Ϣ��SYN=1��seq=x��-----------
	int recv1 = recvfrom(Server_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, &addr_len);
	if (recv1 == 0) return false;
	else if (recv1 > 0) {
		cout << "�������˵�һ�����ֳɹ��յ���";
		if ((message1.Udp_Header.flag & SYN)) {
			if (!check(message1)) return false;
			cout << "����Ϣ׼ȷ����" << endl;
		}
		else return false;
	}
	else return false;

	//----------- ���͵ڶ����ֵ���Ϣ��SYN=1��ACK=1��ack=x��-----------
	message2.Udp_Header.SrcPort = ServerPORT;
	message2.Udp_Header.DestPort = RouterPORT;
	message2.Udp_Header.Seq = now_seq;  //�������ظ���seq=��������seq+1
	message2.Udp_Header.Ack = message1.Udp_Header.Seq+1;  //�������ظ���ack=�ͻ��˷�����seq+1
	now_ack = message2.Udp_Header.Ack;
	message2.Udp_Header.flag += SYN;
	message2.Udp_Header.flag += ACK;
	set_checkNum(message2);//����У���
	int send2 = sendto(Server_Socket, (char*)&message2, sizeof(DATA_PACKAGE), 0, (sockaddr*)&Router_Addr, addr_len);

	now_seq++;
	// ����ʱ�ӣ����ڽ��յĳ�ʱ�ش�
	clock_t message2_start = clock();
	
	//�����û�гɹ�����
	if (send2 == 0) return false;
	cout << "�������˵ڶ������ֳɹ����ͣ�" << endl;

	//----------- ���յ��������ֵ���Ϣ��SYN=1��seq=x��-----------
	//�������̣߳����ڼ��������յ����������֣����߳����ڳ�ʱ�ش�����

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message3, 0, 0);

	//����ͳ�ʱ�ش�
	while (1) {
		//��Ϊ��1��ʱ������յ�����Ϣ
		if (recv_mark == 0) return false;
		//�ɹ��յ���Ϣ
		else if (recv_mark > 0) {
			//�����λ�����к�ack����֤�ɿ��Դ���
			cout << "�������˵��������ֳɹ��յ���";
			if ((message3.Udp_Header.flag & ACK) && (message3.Udp_Header.Ack == message2.Udp_Header.Seq+1)) {
				if (!check(message3)) return false;
				cout << "����Ϣ׼ȷ����" << endl;
				break;
			}
			else return false;
		}
		//�ڶ������ֵ�message2��ʱ��׼���ش�
		if (clock() - message2_start > MaxWaitTime)
		{
			cout << "�ڶ������ֳ�ʱ��׼���ش�......" << endl;
			send2 = sendto(Server_Socket, (char*)&message2, sizeof(DATA_PACKAGE), 0, (sockaddr*)&Router_Addr, addr_len);
			message2_start = clock();
			if (send2 == 0) return false;
		}
	}
	recv_mark = -1;
	cout << "�����������ӳɹ�����ʱTCP���кš�ȷ�Ϻ�Ϊ" << now_seq << " " << now_ack << endl;
	return 1;
}

DWORD WINAPI RecvThread(LPVOID pParam) {
	DATA_PACKAGE* message = (DATA_PACKAGE*)pParam;
	recv_mark = recvfrom(Server_Socket, (char*)message, sizeof(*message), 0, (sockaddr*)&Router_Addr, &addr_len);
	return 0;
}

bool returnACK(DATA_PACKAGE& now_Message) {
	//�̲����˷�װһ������Щ�Ǹ���client��
	DATA_PACKAGE nameMessage = now_Message;
	nameMessage.Udp_Header.SrcPort = ServerPORT;
	nameMessage.Udp_Header.DestPort = RouterPORT;
	nameMessage.Udp_Header.flag |= ACK;
	nameMessage.Udp_Header.Seq = now_seq;
	nameMessage.Udp_Header.Ack = now_Message.Udp_Header.Seq + now_Message.Udp_Header.size;
	nameMessage.Udp_Header.GBN = now_GBN;

	//������ݰ��Ĺ��ɺ�����������У���
	set_checkNum(nameMessage);

	int send0 = sendto(Server_Socket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&Router_Addr, addr_len);
	//�����û�гɹ�����
	if (send0 == 0) return false;
	mtx.lock();
	cout << "���ظ�ACK�������������˳ɹ�����"<< int(nameMessage.Udp_Header.other) <<"�����ݰ���ACK��" << endl;
	cout << "���У���ȷ��GBN���к�Ϊ:" << int(nameMessage.Udp_Header.GBN)<< endl << endl;
	mtx.unlock();
}

bool Data_Recv(DATA_PACKAGE& nameMessage) {
	//1���Ƚ����ļ���message����Ϊ�䷵��һ��ACK
	unsigned int file_size;//�ļ���С
	char file_name[100] = { 0 };//�ļ���
	int rdt_seq = 0; //rdt3.0���
	cout << "���������ݲ�������������0�����ݰ��ɹ��յ���";
	if ((nameMessage.Udp_Header.flag & FILE_TAG) &&(rdt_seq== get_RdtSeq(nameMessage))&& (nameMessage.Udp_Header.Ack == now_seq)) {
		if (!check(nameMessage)) return false;
		cout << endl;
	}

	rdt_seq = !rdt_seq;
	MaxWindowSize = nameMessage.Udp_Header.GBN;
	file_size = nameMessage.Udp_Header.size;
	nameMessage.Udp_Header.size = strlen(nameMessage.Data);
	for (int i = 0; nameMessage.Data[i]; i++)//��ȡ�ļ���
		file_name[i] = nameMessage.Data[i];
	cout << "�յ����ļ����ļ���Ϊ��" << file_name << "����СΪ��" << file_size  << endl;

	//ȷ������󣬵�����ʾ��Ϣ��ѡ��洢Ŀ¼
	cout << "���͵�Ĭ�ϴ洢·����C:\\Users\\34288\\Desktop\\pic\\recv\\" << endl ;

	//����ȷ����Ϣ
	returnACK(nameMessage);

	//2����������Ҫ��ʽ�����ļ��Ĵ���
	int package_num = file_size / MaxMsgSize;//ȫװ���ı��ĸ���
	int left_size = file_size % MaxMsgSize;//����װ����ʣ�౨�Ĵ�С
	char* file_message = new char[file_size];
	cout << "��ʼ�������ݶΣ��� " << package_num + (left_size>0) << " �����Ķ�" << endl;
	cout << package_num;
	//���Ҳ�����µ��߳̽��н��գ���������޸�
	for (int i = 0; i < package_num; i++) {
		DATA_PACKAGE Data_message;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
		while (1) {
			//��Ϊ��1��ʱ������յ�����Ϣ
			if (recv_mark == 0) return false;
			//�ɹ��յ���Ϣ
			else if (recv_mark > 0) {
				//�����λ�����к�ack����֤�ɿ��Դ���
				cout << "���������ݲ������������˳ɹ��յ�" << int(Data_message.Udp_Header.other) << "�����ݰ���" ;
				cout << "���У�GBN���к�Ϊ:" << int(Data_message.Udp_Header.GBN)  << ",�ڴ���GBN���к�Ϊ" << now_GBN << endl;
				if ((Data_message.Udp_Header.GBN == now_GBN) && nameMessage.Udp_Header.other < Data_message.Udp_Header.other) {
					if (!check(Data_message)) return false;
					cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(Data_message) << ",��Ϣ׼ȷ,����" << int(Data_message.Udp_Header.other) << "�����ݰ���" << endl << endl;
					
					//lab2������ά��GBN����nameMessage�ݴ���һ�ν��յ��ļ���Ϣ
					now_GBN = (now_GBN + 1) % MaxWindowSize;
					//ʵ���ϣ������nameMessage���൱�����ǵĽ��ն˻�������Ϊ��СΪ1�Ĵ���
					nameMessage = Data_message;
					returnACK(Data_message);

					for (int j = 0; j < MaxMsgSize; j++)
					{
						file_message[i * MaxMsgSize + j] = Data_message.Data[j];
					}
					break;
				}
				else if ((Data_message.Udp_Header.GBN != now_GBN) && nameMessage.Udp_Header.other < Data_message.Udp_Header.other) {
					if (!check(Data_message)) break;
					cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(Data_message) << "�����֡����ڴ������ݰ�����" << endl << endl;
					returnACK(nameMessage);
					//���½���
					recv_mark = -1;
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
				}
				else {
					cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(Data_message)<< "���յ�С���ۼƷ�Χ�����ݰ�����������"<<endl << endl;
					//���½���
					recv_mark = -1;
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
				}
			}
		}
		recv_mark = -1;
	}
	if (left_size > 0){
		DATA_PACKAGE Data_message;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
		while (1) {
			//��Ϊ��1��ʱ������յ�����Ϣ
			if (recv_mark == 0) return false;
			//�ɹ��յ���Ϣ
			else if (recv_mark > 0) {
				//�����λ�����к�ack����֤�ɿ��Դ���
				cout << "���������ݲ������������˳ɹ��յ�" << int(Data_message.Udp_Header.other) << "�����ݰ���";
				cout << "���У�GBN���к�Ϊ:" << int(Data_message.Udp_Header.GBN) << ",�ڴ���GBN���к�Ϊ" << now_GBN << endl;
				if ((Data_message.Udp_Header.GBN == now_GBN) && nameMessage.Udp_Header.other < Data_message.Udp_Header.other) {
					if (!check(Data_message)) return false;
					cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(Data_message) << ",��Ϣ׼ȷ,����" << int(Data_message.Udp_Header.other) << "�����ݰ���" << endl << endl;
					
					//lab2������ά��GBN����nameMessage�ݴ���һ�ν��յ��ļ���Ϣ
					now_GBN = (now_GBN + 1) % MaxWindowSize;
					nameMessage = Data_message;
					returnACK(Data_message);

					for (int j = 0; j < left_size; j++)
					{
						file_message[package_num * MaxMsgSize + j] = Data_message.Data[j];
					}
					
					recv_mark = -1;
					break;
				}
				else if ((Data_message.Udp_Header.GBN != now_GBN)&& nameMessage.Udp_Header.other < Data_message.Udp_Header.other) {
					if (!check(Data_message)) break;
					cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(Data_message) << "�����֡����ڴ������ݰ�����׼���ظ�ACK��" << endl << endl;
					returnACK(nameMessage);
					recv_mark = -1;
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
				}
				else {
					cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(Data_message) << "���յ�С���ۼƷ�Χ�����ݰ�����������" << endl << endl;
					//���½���
					recv_mark = -1;
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
				}
			}
		}
		recv_mark = -1;
	}

	cout << "�ļ�����ɹ�����ʼд���ļ�......" << endl;
	//д���ļ�
	
	char str[50] = "C:\\Users\\34288\\Desktop\\pic\\recv\\";
	char * path = strcat(str, file_name);
	ofstream fout(path, ofstream::binary);
	if (file_message != 0)
	{
		//if (strstr(file_name, ".txt") != nullptr) {
		//	// ����UTF-8 BOM���ֽ����ǣ�
		//	const unsigned char utf8_bom[3] = { 0xEF, 0xBB, 0xBF };
		//	fout.write(reinterpret_cast<const char*>(utf8_bom), 3);
		//}
		for (int i = 0; i < file_size; i++)
		{
			fout <<file_message[i];
		}
		fout.close();
		cout << "\n�ļ�д��ɹ���" << endl;
	}
	cout << ">>>>>>>>>>>>>>>>>>>>   Data transmission successfully  <<<<<<<<<<<<<<<<<<<<" << endl;
	return true;
}

int main()
{
	// 1����ʼ��Winsock��
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
	// ����Ϊ0ʱ��˵����ʼ���ɹ�
	if (res == 0) {
		cout << "�ɹ���ʼ��Winsock��!" << endl;

		// 2�������������˵�socket
		Server_Socket = socket(AF_INET, SOCK_DGRAM, 0);
		// ����ΪINVALID_SOCKETʱ��˵����ʼ��ʧ�ܣ������쳣����
		if (Server_Socket == INVALID_SOCKET) {
			cout << "������������Socketʧ��!" << endl;
			exit(EXIT_FAILURE);
		}
		else cout << "�ɹ�������������Socket!" << endl;

		// 3������˰�ip��ַ�ͽ��̶˿ں�
		Server_Addr.sin_family = AF_INET;
		// �˿ں�����
		Server_Addr.sin_port = htons(ServerPORT);
		// ip��ַ����Ϊ���ǵĻ���ip��ַ
		inet_pton(AF_INET, "127.0.0.1", &Server_Addr.sin_addr.S_un.S_addr);
		res = bind(Server_Socket, (SOCKADDR*)&Server_Addr, sizeof(SOCKADDR));
		if (res != 0) {
			cout << "�󶨷�������ַʧ��!" << endl;
			exit(EXIT_FAILURE);
		}
		else cout << "�ɹ��󶨷�������ַ!" << endl;

		Router_Addr.sin_family = AF_INET; //��ַ����
		Router_Addr.sin_port = htons(RouterPORT); //�˿ں�
		inet_pton(AF_INET, "127.0.0.1", &Router_Addr.sin_addr.S_un.S_addr);

		cout << ">>>>>>>>>>>>>>>>>>>>   Server get ready  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		// 4���������˽������ӣ���������ʵ����TCP�������ֵ�ģ�⣩
		//=====================��������=====================
		bool ret = TCP_Connect();
		if (ret == 0) {
			cout << "��������:��������Ӧʧ��!" << endl;
			exit(EXIT_FAILURE);
		}

		cout << ">>>>>>>>>>>>>>>>>>>>   Connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		//��������������Ҫ���յ���Ϣ�������ж��ǽ��йر����ӻ����ļ�����
		//�������Ƚ��н�����Ϣ�ͼ�⣬����⵽FIN�����ж��ļ�������ת���Ĵλ���

		// 5���������˴�����Ϣ��������������ͬ����
		DATA_PACKAGE NowMessage;
		int recv = recvfrom(Server_Socket, (char*)&NowMessage, sizeof(NowMessage), 0, (sockaddr*)&Router_Addr, &addr_len);
		while (1) {
			if (recv == 0) {
				cout << "���ݴ���:������Ϣʧ��!" << endl;
			}
			else if (recv > 0) {
				//�ȿ����ǲ���FIN��־������ǽ��뵽�ر�TCP�����д���
				if (NowMessage.Udp_Header.flag & FIN) break;
				else if (NowMessage.Udp_Header.flag & FILE_TAG) {
					int ret = Data_Recv(NowMessage);
					if (ret == false) {
						cout << "���ݴ���:�������ʧ��!" << endl;
						exit(EXIT_FAILURE);
					}
					//������ǰѽ�������ϢŲ������󣬲�������Ϣ�����жϣ������FIN����FILE_TAG���ͽ�����һ������
					//������������õ����ݰ���˵��֮ǰ��ACK��ʧ�ˣ������ش�һ��
					//��Ҫע����ǣ�������ǰͨ�����ô��εķ�ʽ���洢����һ�������DataMessage��NowMessage�С�
					DATA_PACKAGE temp_message = NowMessage;
					cout << "�ݴ���" << int(temp_message.Udp_Header.GBN) << "GBN���кŵ����ݰ�" << endl;
					while (1) {
						recv = recvfrom(Server_Socket, (char*)&NowMessage, sizeof(NowMessage), 0, (sockaddr*)&Router_Addr, &addr_len);
						if (!(NowMessage.Udp_Header.flag & FIN) && !(NowMessage.Udp_Header.flag & FILE_TAG)&& (NowMessage.Udp_Header.other !=0)) {
							cout << "�������˶����յ�" << int(NowMessage.Udp_Header.other) << "�����ݰ���" << now_GBN << int(NowMessage.Udp_Header.GBN);
							if (!check(NowMessage)) break;
							cout << "���֡����ڴ������ݰ�����" << endl;
							returnACK(temp_message);
						}
						//��������״̬������׼����һ�δ���/�Ͽ�
						else {
							now_GBN = 0; //GBN���кŵ�ȷ��
							break;
						}
					}
				}
			}
		}
		ret = Close_TCP_Connect(NowMessage);
		if (ret == false) {
			cout << "�Ĵλ���:��������Ӧʧ��!" << endl;
			exit(EXIT_FAILURE);
		}

		cout << ">>>>>>>>>>>>>>>>>>>>   Close connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		WSACleanup();
	}
	else {
		cout << "��ʼ��Winsock��ʧ��!" << endl;
		exit(EXIT_FAILURE);
	}
	return 0;
}