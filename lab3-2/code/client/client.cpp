#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string.h>
#include<fstream>
#include <mutex>
#include "Message.h"

// ��ַ��C:\Users\34288\Desktop\pic\1.jpg

// lab1���ᵽ����ֹ��������
#pragma comment (lib, "ws2_32.lib")
std::mutex mtx; // ��������Ļ�����

using namespace std;

// ����·�����Ϳͻ��˵Ķ˿ں�
const int RouterPORT = 23381; //·�����˿ں�
const int ClientPORT = 23380; //�ͻ��˶˿ں�

// �����û�socket��·����socket
SOCKET Router_Socket;
SOCKET Client_Socket;

SOCKADDR_IN Router_Addr;
SOCKADDR_IN Client_Addr;

// ȫ�ֱ���
int addr_len = sizeof(Router_Addr);   //��ַ����
int now_seq = 0;    //��ǰ��tcp seq(����)
int now_ack = 0;    //��ǰ��tcp ack(����)

// ���߳������ȫ�ֱ���
int recv_mark = -1;  //���ر��
int wait[MaxWindowSize] = {0};  //ͣ�ȱ��
int	Fin_Trans = 1;	//���������ʶ


// �������������ȫ�ֱ���
// ���յ���GBN���кźͷ��͵�GBN���к�
int recv_GBN = 0;
int send_GBN = 0;
// �������ڻ�ַ����һ��Ҫ���͵����
int window_base = 0;
int next_send = 0;
// ���ͻ�����
DATA_PACKAGE Data_message[MaxWindowSize];

int get_wait() {
	if (wait[send_GBN] == 0) return 1;
	return -1;
}

DWORD WINAPI ReceiveThread(LPVOID pParam) {
	DATA_PACKAGE* message = (DATA_PACKAGE*)pParam;
	recv_mark = recvfrom(Client_Socket, (char*)message, sizeof(*message), 0, (sockaddr*)&Router_Addr,&addr_len);
	return 0;
}

void Send_Again(int start, int num){
	for (int i = 0; i <num; i = i+1) {
		int n = (start + i) % MaxWindowSize;
		if (wait[n] == 1) {
			int send = sendto(Client_Socket, (char*)&Data_message[n], sizeof(Data_message[n]), 0, (sockaddr*)&Router_Addr, addr_len);
			//�����û�гɹ�����
			clock_t message_start = clock();
			if (send == 0) {
				cout << "���ݴ��䣺����ʧ��!" << endl;
				exit(EXIT_FAILURE);
			}
		}
	}
}

DWORD WINAPI GetAckThread() {
	DATA_PACKAGE ACK_message;
	//�������߳������������߳����ڳ�ʱ�ش��ļ�ʱ����Ϣ����
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &ACK_message, 0, 0);

	// ��ʼ��ʱ������ʱ�ӣ����ڽ��յĳ�ʱ�ش�
	clock_t message_start = clock();

	// ���ü��������ڽ��յĿ����ش�
	int counter = 0;

	//����ͳ�ʱ�ش�
	recv_mark = -1;
	while (1) {
		//��Ϊ��1��ʱ������յ�����Ϣ
		if (window_base == next_send && Fin_Trans == 0)break;
		if (recv_mark == 0) return false;
		//�ɹ��յ���Ϣ
		else if (recv_mark > 0) {
			//���д���
			mtx.lock();
			cout << "������ACK�������ͻ��˳ɹ��յ�" << int(ACK_message.Udp_Header.other) << "�����ݰ���ACK��";
			cout << "���У�GBN���к�Ϊ:" << int(ACK_message.Udp_Header.GBN) <<",��ȷ�ϵ�GBN���к�Ϊ" << (recv_GBN+1) % MaxWindowSize<<endl;
			//�յ�δȷ��ACK�����ܴ��ڽϴ�����
			if ((ACK_message.Udp_Header.flag & ACK) && ((ACK_message.Udp_Header.GBN - recv_GBN + MaxWindowSize) % MaxWindowSize) >= 1) {
				cout << "У���Ϊ(��Ϊ1��Ϊ0):" << check(ACK_message) << ",��Ϣ׼ȷ,ȷ��"<< int(ACK_message.Udp_Header.other)<<"�����ݰ��Ѿ����գ�" << endl<<endl;
				mtx.unlock();
				if (!check(ACK_message)) break;
				while (recv_GBN != ACK_message.Udp_Header.GBN) {
					wait[recv_GBN] = 0;
					recv_GBN = (recv_GBN + 1) % MaxWindowSize;
				}
				counter = 0;
				if (window_base == next_send && Fin_Trans == 0)break;
				//�յ��µ�ack�����¿�ʼ��ʱ��
				message_start = clock();
				mtx.lock();
				cout << "������״̬��Ϣ������ ";
				cout << "���ڻ�ַ:" << window_base << " �������͵����ݰ�Ϊ:" << next_send + 1;
				cout << " �ѷ���δȷ�Ϲ���:" << next_send - window_base << " ���ô����͹���:" << MaxWindowSize - (next_send - window_base) << endl << endl;
				mtx.unlock();
			}
			//�յ���ȷ��ACK
			else if ((ACK_message.Udp_Header.flag & ACK) && (recv_GBN % MaxWindowSize) == ACK_message.Udp_Header.GBN) {
				if (!check(ACK_message)) break;
				counter++;
				cout<< "У���Ϊ(��Ϊ1��Ϊ0):" << check(ACK_message) << ",��Ϣ׼ȷ," << "����Ϊ��ȷ�����ݰ���Ŀǰ���յ�" << counter << "��" << endl <<endl;
				if (counter >= 3) {
					cout << ">>>>>>>>׼�������ش����ش�"<< next_send -window_base<<"�����ݰ�<<<<<<<<<" << endl<<endl;
					mtx.unlock();
					Send_Again(recv_GBN, next_send - window_base);
					counter = 0;
				}
				else {
					mtx.unlock();
				}
			}
			//���½���
			recv_mark = -1;
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &ACK_message, 0, 0);
		}
		//��ʱ��׼���ش�
		if (clock() - message_start > MaxWaitTime)
		{
			cout << ">>>>>>>>���ͳ�ʱ��׼���ش�ȫ����Ϣ......<<<<<<<<<" << endl<<endl;
			Send_Again(recv_GBN, MaxWindowSize);
			message_start = clock();
		}
	}
	recv_mark = -1;
	return 0;
}


int SendMyMessage(DATA_PACKAGE& message) {

	int send = sendto(Client_Socket, (char*)&message, sizeof(message), 0, (sockaddr*)&Router_Addr, addr_len);
	//�����û�гɹ�����
	if (send == 0) {
		cout << "���ݴ��䣺����ʧ��!" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		mtx.lock();
		cout << "�����ݷ��Ͳ������ͻ��˳ɹ�����"<< int((message).Udp_Header.other)<<"�����ݰ���" << endl;
		cout << "���У�GBN���к�Ϊ:" << int((message).Udp_Header.GBN) << ",��ȷ�ϵ�GBN���к�Ϊ" << (recv_GBN) % MaxWindowSize << endl << endl;
		mtx.unlock();
		next_send++;
		mtx.lock();
		cout << "������״̬��Ϣ������";
		if((window_base == next_send)&& (window_base == 0)) cout << "�����Ѿ���������ʱ�������Ƶ������������Ϣ";
		else cout << "���ڻ�ַ:" << window_base << " �������͵����ݰ�Ϊ:" << next_send + 1;
		cout << " �ѷ���δȷ�Ϲ���:" << next_send - window_base << " ���ô����͹���:" << MaxWindowSize - (next_send - window_base)<< endl << endl;
		mtx.unlock();
	
	}
}

bool Close_TCP_Connect(){
	DATA_PACKAGE message1;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;
	DATA_PACKAGE message4;

	//-----------��һ�λ��ִ���ʼ��FIN=1��seq=u��-----------
	message1.Udp_Header.SrcPort = ClientPORT;
	message1.Udp_Header.DestPort = RouterPORT;
	message1.Udp_Header.flag += FIN;//����FIN
	message1.Udp_Header.Seq = now_seq++;//�������seq
	message1.Udp_Header.Ack = now_ack;

	set_checkNum(message1);
	int send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
	
	// ����ʱ�ӣ����ڽ��յĳ�ʱ�ش�
	clock_t message1_start = clock();

	//�����û�гɹ�����
	if (send1 == 0) return false;
	cout << "�ͻ��˵�һ�λ��ֳɹ����ͣ�" << endl;

	//-----------�ڶ��λ��ִ���ʼ��ACK=1��seq =v ��ack=u+1��-----------
	//�������̣߳����ڼ��������յ��ڶ������֣����߳����ڳ�ʱ�ش�����
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message2, 0, 0);

	//����ͳ�ʱ�ش�
	while (1) {
		//��Ϊ��1��ʱ������յ�����Ϣ
		if (recv_mark == 0) return false;
		//�ɹ��յ���Ϣ
		else if (recv_mark > 0) {
			//�����λ�����к�ack����֤�ɿ��Դ���
			cout << "�ͻ��˵ڶ��λ��ֳɹ��յ���";
			if ((message2.Udp_Header.flag & ACK) && (message2.Udp_Header.Ack == message1.Udp_Header.Seq + 1)) {
				if (!check(message2)) return false;
				cout << "����Ϣ׼ȷ����" << endl;
				break;
			}
			else return false;
		}
		//��һ�����ֵ�message1��ʱ��׼���ش�
		if (clock() - message1_start > MaxWaitTime)
		{
			cout << "��һ�λ��ֳ�ʱ��׼���ش�......" << endl;
			send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
			message1_start = clock();
			if (send1 == 0) return false;
		}
	}
	recv_mark = -1;

	//-----------�����λ��ִ���ʼ��FIN=1��ACK=1��seq=w��ack=u+1��-----------
	recv_mark = recvfrom(Client_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, &addr_len);
	//��Ϊ��1��ʱ������յ�����Ϣ
	if (recv_mark == 0) return false;
	//�ɹ��յ���Ϣ
	else if (recv_mark > 0) {
		//�����λ�����к�ack����֤�ɿ��Դ���
		cout << "�ͻ��˵����λ��ֳɹ��յ���";
		if ((message3.Udp_Header.flag & ACK) && (message3.Udp_Header.flag & FIN)&& (message3.Udp_Header.Ack == message1.Udp_Header.Seq + 1)) {
			if (!check(message3)) return false;
			cout << "����Ϣ׼ȷ����" << endl;
		}
		else return false;
	}
	recv_mark = -1;

	//-----------���Ĵλ��ִ���ʼ��ACK=1��seq=u+1��ack = w+1��-----------
	message4.Udp_Header.SrcPort = ClientPORT;
	message4.Udp_Header.DestPort = RouterPORT;
	message4.Udp_Header.flag += ACK;//����ACK
	message4.Udp_Header.Seq = now_seq;//�������seq=u+1;
	message4.Udp_Header.Ack = message3.Udp_Header.Seq + 1;
	now_ack = message4.Udp_Header.Ack;
	set_checkNum(message4);//����У���
	int send4 = sendto(Client_Socket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&Router_Addr, addr_len);
	if (send4 == 0)
	{
		return false;
	}
	cout << "�ͻ��˵��Ĵλ��ֳɹ����ͣ�" << endl;

	//-----------�ȴ�2MSL����ֹACK��ʧ-----------
	int close_clock = clock();
	cout << "�ͻ��˽���2MSL�ȴ�..." << endl;
	//���յ���ʾ��Ϣ�����·���
	DATA_PACKAGE tmp;
	
	//�������Ľ����߳��߳�
	HANDLE wait = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &tmp, 0, 0);

	while (clock() - close_clock < 2 * MaxWaitTime)
	{
		if (recv_mark == 0) return false;
		else if (recv_mark > 0)
		{
			//��ʧ��ack
			send4 = sendto(Client_Socket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&Router_Addr, addr_len);
			cout << "�쳣�ָ������·��ͽ���ACK" << endl;
		}
	}

	//�����߳�
	TerminateThread(wait, EXIT_FAILURE);
	cout << "�ͻ��˹ر����ӳɹ�����ʱTCP���кš�ȷ�Ϻ�Ϊ" << now_seq << " " << now_ack << endl;
}

bool TCP_Connect(){
	DATA_PACKAGE message1;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;
	//-----------��һ�����ִ���ʼ��SYN=1��seq=x��-----------
	//ʵ����Ӧ���������һ��seq���������
	message1.Udp_Header.SrcPort = ClientPORT;
	message1.Udp_Header.DestPort = RouterPORT;
	message1.Udp_Header.flag += SYN;//����SYN
	message1.Udp_Header.Seq = now_seq++;//�������seq
	set_checkNum(message1);//����У���

	int send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
	
	// ����ʱ�ӣ����ڽ��յĳ�ʱ�ش�
	clock_t message1_start = clock();

	//�����û�гɹ�����
	if (send1 == 0) return false;
	cout << "�ͻ��˵�һ�����ֳɹ����ͣ�" << endl;

	//-----------�ڶ������ִ���ʼ��SYN=1��ACK=1��ack=x��-----------
	//�������̣߳����ڼ��������յ��ڶ������֣����߳����ڳ�ʱ�ش�����
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message2, 0, 0);

	//����ͳ�ʱ�ش�
	while (1){
		//��Ϊ��1��ʱ������յ�����Ϣ
		if (recv_mark == 0) return false;
		//�ɹ��յ���Ϣ
		else if (recv_mark > 0){
			//�����λ�����к�ack����֤�ɿ��Դ���
			cout << "�ͻ��˵ڶ������ֳɹ��յ���";
			if ((message2.Udp_Header.flag & ACK) && (message2.Udp_Header.flag & SYN)  && (message2.Udp_Header.Ack == message1.Udp_Header.Seq+1)){
				if (!check(message2)) return false;
				cout << "����Ϣ׼ȷ����" << endl;
				break;
			}
			else return false;
		}
		//��һ�����ֵ�message1��ʱ��׼���ش�
		if (clock() - message1_start > MaxWaitTime)
		{
			cout << "��һ�����ֳ�ʱ��׼���ش�......" << endl;
			send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
			message1_start = clock();
			if (send1 == 0) return false;
		}
	}
	recv_mark = -1;

	//-----------���������ִ���ʼ��ACK=1��seq=x+1��-----------
	message3.Udp_Header.SrcPort = ClientPORT;
	message3.Udp_Header.DestPort = RouterPORT;
	message3.Udp_Header.flag += ACK;//����ACK
	message3.Udp_Header.Seq = now_seq;//�������seq=x+1
	message3.Udp_Header.Ack = message2.Udp_Header.Seq + 1;
	now_ack = message3.Udp_Header.Ack;
	set_checkNum(message3);//����У���
	int send3 = sendto(Client_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, addr_len);
	if (send3 == 0)
	{
		return false;
	}
	cout << "�ͻ��˵����λ��ֳɹ����ͣ�" << endl;
	cout << "�ͻ������ӳɹ�����ʱTCP���кš�ȷ�Ϻ�Ϊ" <<now_seq<<" "<<now_ack << endl;
	return true;
}

bool Data_Send(string filename) {
	int rdt_seq = 0; //��¼��ǰrdt_seq

	//1�������ļ�·������ȡ�ļ���
	string realname = "";
	for (int i = filename.size() - 1; i >= 0; i--)
	{
		if (filename[i] == '/' || filename[i] == '\\') break;
		realname += filename[i];
	}
	realname = string(realname.rbegin(), realname.rend());

	//2�����ļ������ļ�תΪ�ֽ�����д��filemessage
	ifstream fin(filename.c_str(), ifstream::binary);
	if (!fin) {
		printf("�޷����ļ���\n");
		return false;
	}

	char* filemessage = new char[MaxFileSize];
	int file_size = 0;
	while (fin) {
		filemessage[file_size] = fin.get();
		file_size++;
	}
	file_size--;
	fin.close();

	//3������Ԥ����Ϣ����֪���ն��ļ������ļ���Ϣ���Լ����ڴ�С
	DATA_PACKAGE nameMessage;
	nameMessage.Udp_Header.SrcPort = ClientPORT;
	nameMessage.Udp_Header.DestPort = RouterPORT;
	//��flag����FILE��sizeλ����ı䣬�����ݰ���С��Ϊ�����ļ���С
	nameMessage.Udp_Header.size = file_size;
	nameMessage.Udp_Header.flag += FILE_TAG;
	nameMessage.Udp_Header.Seq = now_seq;
	nameMessage.Udp_Header.Ack = now_ack;
	nameMessage.Udp_Header.GBN = MaxWindowSize;
	nameMessage.Udp_Header.other = 0;

	for (int i = 0; i < realname.size(); i++)//��䱨�����ݶ�
		nameMessage.Data[i] = realname[i];
	nameMessage.Data[realname.size()] = '\0';//�ַ�����β��\0


	//������ݰ��Ĺ��ɺ�����������У���
	set_checkNum(nameMessage);
	int send0 = sendto(Client_Socket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&Router_Addr, addr_len);
	Data_message[0] = nameMessage;
	wait[0] = 1;

	// ����ʱ�ӣ����ڽ��յĳ�ʱ�ش�
	clock_t message0_start = clock();

	//�����û�гɹ�����
	if (send0 == 0) return false;
	cout << "�ͻ��˳ɹ�����0�����ݰ�(�ļ������ļ���Ϣ)��" << endl<<endl;

	//��ʱ��⣺�������������û���յ�������д�������Ҳ���ʮ�ַ�ŭ����֪���ѷ��ͺͳ�ʱ����װһ����
	//д����������Ͼ�ȥ��װ���̲����ˡ�

	DATA_PACKAGE recv_message;
	//�������̣߳����ڼ��������յ�ACK�����߳����ڳ�ʱ�ش�����
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &recv_message, 0, 0);

	while (1) {
		//��Ϊ��1��ʱ������յ�����Ϣ
		if (recv_mark == 0) return false;
		//�ɹ��յ���Ϣ
		else if (recv_mark > 0) {
			//�����λ�����к�ack����֤�ɿ��Դ���
			cout << "�ͻ��˳ɹ��յ�"<<int(recv_message.Udp_Header.other)<<"�����ݰ���ACK��"<<endl<<endl;
			if ((recv_message.Udp_Header.flag & ACK)) {
				if (!check(recv_message)) return false;
#ifdef test
				cout << "����Ϣ׼ȷ����" << endl<<"���У�RDTȷ�Ϻ�Ϊ:" << get_RdtAck(recv_message) << ",У���Ϊ(��Ϊ1��Ϊ0):" << check(recv_message);
				cout << ",TCP���к�Ϊ:" << recv_message.Udp_Header.Seq << ",TCPȷ�Ϻ�Ϊ:" << recv_message.Udp_Header.Ack << endl<<endl;
#else 
				cout << endl;
#endif 
				wait[0] = 0;
				break;
			}
			else return false;
		}
		//message��ʱ��׼���ش�
		if (clock() - message0_start > MaxWaitTime)
		{
			cout << "0�����ݰ���ʱ��׼���ش�......" << endl;
			send0 = sendto(Client_Socket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&Router_Addr, addr_len);
			message0_start = clock();
			if (send0 == 0) return false;
		}
	}
	recv_mark = -1;
	Fin_Trans = 1;

	//4���������������������ͬ�⣨ʵ����û�����飩���Ϳ��Է���ȫ���������ˣ�������У�֮������ⲿ���Ż�Ϊ��������
	//Ϊ��Ϊ֮����׼��������Ҳ�ö��߳�ʵ�֣�ʵ����ͣ�Ȼ��ƿ��Բ������̡߳�

	int package_num = file_size / MaxMsgSize;//ȫװ���ı��ĸ���
	int left_size = file_size % MaxMsgSize;//����װ����ʣ�౨�Ĵ�С

	// ��ʼ���㴫���ʱ��
	int start_time = clock(); //����������

	// ����Ψһ����Ϣ�����߳�
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GetAckThread,0, 0, 0);


	//���ν������ͣ������ظ�ȫ������Ӧ�߳�ʵ��
	for (int i = 0; i < package_num; i++) {
		//�ȴ����գ����������ڵĵȴ�ʵ��
		while (next_send - window_base >= MaxWindowSize || get_wait() < 0) {
			if (get_wait()==1) {
				window_base++;
				break;
			}
		}
		int num = send_GBN;
		Data_message[num].Udp_Header.SrcPort = ClientPORT;
		Data_message[num].Udp_Header.DestPort = RouterPORT;
		Data_message[num].Udp_Header.Seq = now_seq;
		Data_message[num].Udp_Header.Ack = now_ack;

		//lab3-2������GBN���кŵ�ά��
		Data_message[num].Udp_Header.GBN = num;
		wait[num] = 1;
		send_GBN = (send_GBN + 1) % MaxWindowSize;

		if (rdt_seq == 0) {
			Data_message[num].Udp_Header.flag += RDT_SEQ;
			rdt_seq = 1;
		}
		else rdt_seq = 0;
		Data_message[num].Udp_Header.other = i+1;


		for (int j = 0; j < MaxMsgSize; j++)
		{
			Data_message[num].Data[j] = filemessage[i * MaxMsgSize + j];
		}
		Data_message[num].Udp_Header.size = MaxMsgSize;

		set_checkNum(Data_message[num]);
		SendMyMessage(Data_message[num]);
	}
	recv_mark = -1;

	//ʣ�ಿ��
	if (left_size > 0)
	{
		//�ȴ����գ����������ڵĵȴ�ʵ��
		while (next_send - window_base >= MaxWindowSize) {
			if (get_wait() >= 0) {
				window_base++;
				break;
			}
		}
		int num = send_GBN;
		Data_message[num].Udp_Header.SrcPort = ClientPORT;
		Data_message[num].Udp_Header.DestPort = RouterPORT;
		Data_message[num].Udp_Header.Seq = now_seq;
		Data_message[num].Udp_Header.Ack = now_ack;

		//lab3-2������GBN���кŵ�ά��
		Data_message[num].Udp_Header.GBN = num;
		wait[num] = 1;
		send_GBN = (send_GBN + 1) % MaxWindowSize;

		if (rdt_seq == 0) {
			Data_message[num].Udp_Header.flag += RDT_SEQ;
			rdt_seq = 1;
		}
		else rdt_seq = 0;
		Data_message[num].Udp_Header.other = package_num + 1;

		for (int j = 0; j < left_size; j++)
		{
			Data_message[num].Data[j] = filemessage[package_num * MaxMsgSize + j];
		}
		Data_message[num].Udp_Header.size = left_size;
		set_checkNum(Data_message[num]);
		next_send--;
		SendMyMessage(Data_message[num]);
	}

	//�ȴ�ȫ���������,���ò�����
	while (Fin_Trans) {
		if (get_wait() >= 0) {
			window_base++;
			send_GBN = (send_GBN + 1) % MaxWindowSize;
			if (window_base == next_send) {
				Fin_Trans = 0;
				recv_GBN = 0;
				send_GBN = 0;
				// �������ڻ�ַ����һ��Ҫ���͵����
				window_base = 0;
				next_send = 0;
				break;
			}
		}
	}

	//���㴫��ʱ���������
	int end_time = clock();
	cout << ">>>>>>>>>>>>>>>>>>>>   Data transmission successfully  <<<<<<<<<<<<<<<<<<<<" << endl;
	cout << "���崫��ʱ��Ϊ:" << (end_time - start_time) / CLOCKS_PER_SEC << "s" << endl;
	cout << "������:" << ((float)file_size) / ((end_time - start_time) / CLOCKS_PER_SEC) << "byte/s" << endl << endl;

	return true;
}

int main()
{
	// ����������lab1�Ĳ����ʼ���ͻ��˻���

	// 1����ʼ��Winsock��
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);

	if (res == 0) {
		cout << "�ɹ���ʼ��Winsock��!" << endl;

		// 2�������û��˵�socket���󶨵�ַ
		Client_Socket = socket(AF_INET, SOCK_DGRAM, 0);
		Client_Addr.sin_family = AF_INET;
		// �˿ں�����
		Client_Addr.sin_port = htons(ClientPORT);
		// ip��ַ����Ϊ���Ǳ�����ip��ַ
		inet_pton(AF_INET, "127.0.0.1", &Client_Addr.sin_addr.S_un.S_addr);
		bind(Client_Socket, (LPSOCKADDR)&Client_Addr, sizeof(Client_Addr));

		// ����ΪINVALID_SOCKETʱ��˵����ʼ��ʧ�ܣ������쳣����
		if (Client_Socket == INVALID_SOCKET) {
			cout << "�����û���Socketʧ��!" << endl;
			exit(EXIT_FAILURE);
		}
		else cout << "�ɹ������û���Socket!" << endl;

		// 3����·������ip��ַ�ͽ��̶˿ں�
		Router_Addr.sin_family = AF_INET;
		// �˿ں�����
		Router_Addr.sin_port = htons(RouterPORT);
		// ip��ַ����Ϊ���Ǳ�����ip��ַ
		inet_pton(AF_INET, "127.0.0.1", &Router_Addr.sin_addr.S_un.S_addr);

		cout << ">>>>>>>>>>>>>>>>>>>>   Client get ready  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		// 4�������������������������ʵ����TCP�������ֵ�ģ�⣩
		bool ret = TCP_Connect();
		if (ret == false) {
			cout << "��������:���ӷ�����ʧ��!" << endl;
			exit(EXIT_FAILURE);
		}
		
		cout << ">>>>>>>>>>>>>>>>>>>>   Connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		// 5�������û�ָ����ѡ��������ж�
		while (1)
		{
			int choice;
			cout << "���������Ĳ�����" << endl << "����ֹ���ӡ���1		�����ļ�����0��" << endl;
			cin >> choice;
			if (choice == 1)break;
			else if (choice == 0) {
				string filename;
				cout << "�������ļ�·����" << endl;
				cin >> filename;
				if (!Data_Send(filename)) {
					cout << "���ݴ������������" << endl << endl;
				}
			}
		}

		// 6�������Ĵλ��ֶϿ�����
		ret = Close_TCP_Connect();
		if (ret == false) {
			cout << "�Ĵλ���:�Ͽ�������ʧ��!" << endl;
			exit(EXIT_FAILURE);
		}

		cout << ">>>>>>>>>>>>>>>>>>>>   Close connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		closesocket(Client_Socket);
		WSACleanup();
	}
	else {
		cout << "��ʼ��Winsock��ʧ��!" << endl;
		exit(EXIT_FAILURE);
	}
}