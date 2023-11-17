#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <fstream>

#include "Message.h"

// lab1中提到，防止函数报错
#pragma comment (lib, "ws2_32.lib")

#define test 0

using namespace std;

// 设置路由器和服务器端的端口号
const int RouterPORT = 23381; //路由器端口号
const int ServerPORT = 23382; //客户端端口号

// 设置服务器socket和路由器socket
SOCKET Router_Socket;
SOCKET Server_Socket;

SOCKADDR_IN Router_Addr;
SOCKADDR_IN Server_Addr;

// 全局变量
int recv_mark = -1;  //返回标记
int addr_len = sizeof(Router_Addr);   //地址长度
//为了区分，我们服务器的序列号从1000开始
int now_seq = 1000;  //服务器端的序列号
int now_ack = 0; //服务器端的确认号
int data_state = 0; //数据传输状态


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

	//----------- 处理第一次挥手的消息（FIN=1，seq=u）-----------
	cout << "服务器端第一次挥手成功收到！" ;
	//检查标记位
	if ((message1.Udp_Header.flag & FIN) && (message1.Udp_Header.Ack == now_seq)){
		if (!check(message1)) return false;
		cout << "且信息准确无误" << endl;
	}
	else return false;



	//----------- 发送第二次挥手的消息（ACK=1，seq =v ，ack=u+1）-----------
	message2.Udp_Header.SrcPort = ServerPORT;
	message2.Udp_Header.DestPort = RouterPORT;
	message2.Udp_Header.Ack = message1.Udp_Header.Seq+1;
	message2.Udp_Header.Seq = now_seq;
	message2.Udp_Header.flag += ACK;
	set_checkNum(message2);//设置校验和

	int send2 = sendto(Server_Socket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&Router_Addr, addr_len);

	//检查有没有成功发送
	if (send2 == 0) return false;
	cout << "服务器端第二次挥手成功发送！" << endl;

	//----------- 等待数据传输完毕-----------
	while (1) {
		if (data_state == 0)break;
	}

	//----------- 发送第三次挥手的消息（FIN=1，ACK=1，seq=w，ack=u+1）-----------

	message3.Udp_Header.SrcPort = ServerPORT;
	message3.Udp_Header.DestPort = RouterPORT;
	message3.Udp_Header.Ack = message1.Udp_Header.Seq+1;
	message3.Udp_Header.Seq = now_seq;
	message3.Udp_Header.flag += ACK;
	message3.Udp_Header.flag += FIN;
	now_ack = message3.Udp_Header.Ack;
	set_checkNum(message3);//设置校验和

	int send3 = sendto(Server_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, addr_len);
	clock_t message3_start = clock();

	//检查有没有成功发送
	if (send3 == 0) return false;
	cout << "服务器端第三次挥手成功发送！" << endl;

	//建立新线程，用于监听有无收到第二次握手，主线程用于超时重传处理
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message4, 0, 0);

	//----------- 接收第四次挥手的消息（ACK=1，seq=u+1，ack = w+1）-----------
	now_seq++;
	//检验和超时重传
	while (1) {
		//不为负1的时候表明收到了消息
		if (recv_mark == 0) return false;
		//成功收到消息
		else if (recv_mark > 0) {
			//检查标记位和序列号ack，保证可靠性传输
			cout << "服务器端第四次挥手成功收到！";
			if ((message4.Udp_Header.flag & ACK) && (message4.Udp_Header.Ack == message3.Udp_Header.Seq + 1)) {
				if (!check(message4)) return false;
				cout << "且信息准确无误" << endl;
				break;
			}
			else return false;
		}
		//message3超时，准备重传
		if (clock() - message3_start > MaxWaitTime)
		{
			cout << "第三次挥手超时，准备重传......" << endl;
			send3 = sendto(Server_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, addr_len);
			message3_start = clock();
			if (send3 == 0) return false;
		}
	}
	recv_mark = -1;

	cout << "服务器端关闭连接成功！此时TCP序列号、确认号为" << now_seq << " " << now_ack << endl;
}

bool TCP_Connect() {
	DATA_PACKAGE message1;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;


	//----------- 接收第一次握手的消息（SYN=1，seq=x）-----------
	int recv1 = recvfrom(Server_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, &addr_len);
	if (recv1 == 0) return false;
	else if (recv1 > 0) {
		cout << "服务器端第一次握手成功收到！";
		if ((message1.Udp_Header.flag & SYN)) {
			if (!check(message1)) return false;
			cout << "且信息准确无误" << endl;
		}
		else return false;
	}
	else return false;

	//----------- 发送第二握手的消息（SYN=1，ACK=1，ack=x）-----------
	message2.Udp_Header.SrcPort = ServerPORT;
	message2.Udp_Header.DestPort = RouterPORT;
	message2.Udp_Header.Seq = now_seq;  //服务器回复的seq=服务器的seq+1
	message2.Udp_Header.Ack = message1.Udp_Header.Seq+1;  //服务器回复的ack=客户端发来的seq+1
	now_ack = message2.Udp_Header.Ack;
	message2.Udp_Header.flag += SYN;
	message2.Udp_Header.flag += ACK;
	set_checkNum(message2);//设置校验和
	int send2 = sendto(Server_Socket, (char*)&message2, sizeof(DATA_PACKAGE), 0, (sockaddr*)&Router_Addr, addr_len);

	now_seq++;
	// 设置时钟，用于接收的超时重传
	clock_t message2_start = clock();
	
	//检查有没有成功发送
	if (send2 == 0) return false;
	cout << "服务器端第二次握手成功发送！" << endl;

	//----------- 接收第三次握手的消息（SYN=1，seq=x）-----------
	//建立新线程，用于监听有无收到第三次握手，主线程用于超时重传处理

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message3, 0, 0);

	//检验和超时重传
	while (1) {
		//不为负1的时候表明收到了消息
		if (recv_mark == 0) return false;
		//成功收到消息
		else if (recv_mark > 0) {
			//检查标记位和序列号ack，保证可靠性传输
			cout << "服务器端第三次握手成功收到！";
			if ((message3.Udp_Header.flag & ACK) && (message3.Udp_Header.Ack == message2.Udp_Header.Seq+1)) {
				if (!check(message3)) return false;
				cout << "且信息准确无误" << endl;
				break;
			}
			else return false;
		}
		//第二次握手的message2超时，准备重传
		if (clock() - message2_start > MaxWaitTime)
		{
			cout << "第二次握手超时，准备重传......" << endl;
			send2 = sendto(Server_Socket, (char*)&message2, sizeof(DATA_PACKAGE), 0, (sockaddr*)&Router_Addr, addr_len);
			message2_start = clock();
			if (send2 == 0) return false;
		}
	}
	recv_mark = -1;
	cout << "服务器端连接成功！此时TCP序列号、确认号为" << now_seq << " " << now_ack << endl;
	return 1;
}

DWORD WINAPI RecvThread(LPVOID pParam) {
	DATA_PACKAGE* message = (DATA_PACKAGE*)pParam;
	recv_mark = recvfrom(Server_Socket, (char*)message, sizeof(*message), 0, (sockaddr*)&Router_Addr, &addr_len);
	return 0;
}

bool returnACK(DATA_PACKAGE& now_Message) {
	//忍不了了封装一个，这些是复制client的
	DATA_PACKAGE nameMessage = now_Message;
	nameMessage.Udp_Header.SrcPort = ServerPORT;
	nameMessage.Udp_Header.DestPort = RouterPORT;
	nameMessage.Udp_Header.flag |= ACK;
	nameMessage.Udp_Header.Seq = now_seq;
	nameMessage.Udp_Header.Ack = now_Message.Udp_Header.Seq + now_Message.Udp_Header.size;

	now_seq++;

	//完成数据包的构成后不能忘了设置校验和
	set_checkNum(nameMessage);
	int send0 = sendto(Server_Socket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&Router_Addr, addr_len);
	//检查有没有成功发送
	if (send0 == 0) return false;
	cout << "服务器端成功发送"<< int(nameMessage.Udp_Header.other) <<"号数据包的ACK！" << endl;
#ifdef test ==1
	cout << "其中：RDT确认号为:" << get_RdtAck(nameMessage) << ",校验和为(对为1错为0):" << check(nameMessage);
	cout << ",TCP序列号为:" << nameMessage.Udp_Header.Seq << ",TCP确认号为:" << nameMessage.Udp_Header.Ack << endl << endl;
#else 
	cout << endl;
#endif 
}

bool Data_Recv(DATA_PACKAGE& nameMessage) {
	//1、先解析文件名message，并为其返回一个ACK
	unsigned int file_size;//文件大小
	char file_name[100] = { 0 };//文件名
	int rdt_seq = 0; //rdt3.0序号
	cout << "服务器端0号数据包成功收到！";
	if ((nameMessage.Udp_Header.flag & FILE_TAG) &&(rdt_seq== get_RdtSeq(nameMessage))&& (nameMessage.Udp_Header.Ack == now_seq)) {
		if (!check(nameMessage)) return false;
#ifdef test ==1
		cout << "且信息准确无误" << endl << "其中：RDT序列号为:" << get_RdtSeq(nameMessage);
		cout << ",校验和为(对为1错为0):" << check(nameMessage);
		cout << ",TCP序列号为:" << nameMessage.Udp_Header.Seq << ",TCP确认号为:" << nameMessage.Udp_Header.Ack << endl;
#else 
		cout << endl;
#endif
	}

	rdt_seq = !rdt_seq;
	file_size = nameMessage.Udp_Header.size;
	nameMessage.Udp_Header.size = strlen(nameMessage.Data);
	for (int i = 0; nameMessage.Data[i]; i++)//获取文件名
		file_name[i] = nameMessage.Data[i];
	cout << "收到新文件！文件名为：" << file_name << "，大小为：" << file_size  << endl;

	//确认无误后，弹出提示信息，选择存储目录
	cout << "请输入文件存储目录：" << endl ;
	char* path= new char[100];
	cin >> path;

	//返回确认信息
	returnACK(nameMessage);

	//2、接下来，要正式进行文件的传输
	int package_num = file_size / MaxMsgSize;//全装满的报文个数
	int left_size = file_size % MaxMsgSize;//不能装满的剩余报文大小
	char* file_message = new char[file_size];
	cout << "开始接收数据段，共 " << package_num + (left_size>0) << " 个报文段" << endl;
	cout << package_num;
	//这里，也开了新的线程进行接收，方便后面修改
	for (int i = 0; i < package_num; i++) {
		DATA_PACKAGE Data_message;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
		while (1) {
			//不为负1的时候表明收到了消息
			if (recv_mark == 0) return false;
			//成功收到消息
			else if (recv_mark > 0) {
				//检查标记位和序列号ack，保证可靠性传输
				cout << "服务器端成功收到" << int(Data_message.Udp_Header.other) << "号数据包！";
				if ((rdt_seq == get_RdtSeq(Data_message)) && (Data_message.Udp_Header.Ack == now_seq)) {
					if (!check(Data_message)) return false;
#ifdef test
					cout << "且信息准确无误" << endl << "其中：RDT序列号为:" << get_RdtSeq(Data_message) << ",校验和为(对为1错为0):" << check(Data_message);
					cout << ",TCP序列号为:" << Data_message.Udp_Header.Seq << ",TCP确认号为:" << Data_message.Udp_Header.Ack << endl;
#else 
					cout << endl;
#endif 
					//返回确认信息
					returnACK(Data_message);
					rdt_seq = !rdt_seq;
					for (int j = 0; j < MaxMsgSize; j++)
					{
						file_message[i * MaxMsgSize + j] = Data_message.Data[j];
					}
					break;
				}
				else if ((rdt_seq != get_RdtSeq(Data_message)) || (Data_message.Udp_Header.Ack < now_seq)) {
					if (!check(Data_message)) break;
#ifdef test
					cout << "出现【重复接收报文段】！" << endl << "其中：RDT序列号为:" << get_RdtSeq(Data_message) << ",校验和为(对为1错为0):" << check(Data_message);
					cout << ",TCP序列号为:" << Data_message.Udp_Header.Seq << ",TCP确认号为:" << Data_message.Udp_Header.Ack << endl;
#endif 		
					i--;
					break;
				}
				else {
					cout << "信息不准确";
					return false;
				}
			}
		}
		recv_mark = -1;
	}
	if (left_size > 0){
		DATA_PACKAGE Data_message;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
		while (1) {
			//不为负1的时候表明收到了消息
			if (recv_mark == 0) return false;
			//成功收到消息
			else if (recv_mark > 0) {
				cout << "服务器端成功收到" << int(Data_message.Udp_Header.other) << "号数据包！";
				//检查标记位和序列号ack，保证可靠性传输
				if ((rdt_seq == get_RdtSeq(Data_message)) && (Data_message.Udp_Header.Ack == now_seq)) {
					if (!check(Data_message)) return false;
#ifdef test
					cout << "且信息准确无误" << endl << "其中：RDT序列号为:" << get_RdtSeq(Data_message) << ",校验和为(对为1错为0):" << check(Data_message);
					cout << ",TCP序列号为:" << Data_message.Udp_Header.Seq << ",TCP确认号为:" << Data_message.Udp_Header.Ack << endl;
#else 
					cout << endl;
#endif 
					//返回确认信息
					returnACK(Data_message);
					rdt_seq = !rdt_seq;
					for (int j = 0; j < left_size; j++)
					{
						file_message[package_num * MaxMsgSize + j] = Data_message.Data[j];
					}
					break;
				}
				else if ((rdt_seq != get_RdtSeq(Data_message)) || (Data_message.Udp_Header.Ack < now_seq)) {
					if (!check(Data_message)) break;
#ifdef test ==1
					cout << "出现【重复接收报文段】！" << endl << "其中：RDT序列号为:" << get_RdtSeq(Data_message) << ",校验和为(对为1错为0):" << check(Data_message);
					cout << ",TCP序列号为:" << Data_message.Udp_Header.Seq << ",TCP确认号为:" << Data_message.Udp_Header.Ack << endl;
#endif 
					break;
				}
				else {
					cout << "信息不准确";
					return false;
				}
			}
		}
		recv_mark = -1;
	}

	cout << "文件传输成功，开始写入文件......" << endl;
	//写入文件
	
	path = strcat(path, file_name);
	ofstream fout(path, ofstream::binary);
	if (file_message != 0)
	{
		//if (strstr(file_name, ".txt") != nullptr) {
		//	// 设置UTF-8 BOM（字节序标记）
		//	const unsigned char utf8_bom[3] = { 0xEF, 0xBB, 0xBF };
		//	fout.write(reinterpret_cast<const char*>(utf8_bom), 3);
		//}
		for (int i = 0; i < file_size; i++)
		{
			fout <<file_message[i];
		}
		fout.close();
		cout << "\n文件写入成功！" << endl;
	}

	cout << ">>>>>>>>>>>>>>>>>>>>   Data transmission successfully  <<<<<<<<<<<<<<<<<<<<" << endl;
	return true;
}

int main()
{
	// 1、初始化Winsock库
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
	// 返回为0时候，说明初始化成功
	if (res == 0) {
		cout << "成功初始化Winsock库!" << endl;

		// 2、创建服务器端的socket
		Server_Socket = socket(AF_INET, SOCK_DGRAM, 0);
		// 返回为INVALID_SOCKET时候，说明初始化失败，进行异常处理
		if (Server_Socket == INVALID_SOCKET) {
			cout << "创建服务器端Socket失败!" << endl;
			exit(EXIT_FAILURE);
		}
		else cout << "成功创建服务器端Socket!" << endl;

		// 3、服务端绑定ip地址和进程端口号
		Server_Addr.sin_family = AF_INET;
		// 端口号设置
		Server_Addr.sin_port = htons(ServerPORT);
		// ip地址设置为我们的环回ip地址
		inet_pton(AF_INET, "127.0.0.1", &Server_Addr.sin_addr.S_un.S_addr);
		res = bind(Server_Socket, (SOCKADDR*)&Server_Addr, sizeof(SOCKADDR));
		if (res != 0) {
			cout << "绑定服务器地址失败!" << endl;
			exit(EXIT_FAILURE);
		}
		else cout << "成功绑定服务器地址!" << endl;

		Router_Addr.sin_family = AF_INET; //地址类型
		Router_Addr.sin_port = htons(RouterPORT); //端口号
		inet_pton(AF_INET, "127.0.0.1", &Router_Addr.sin_addr.S_un.S_addr);

		cout << ">>>>>>>>>>>>>>>>>>>>   Server get ready  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		// 4、服务器端建立连接（这里我们实现了TCP三次握手的模拟）
		//=====================建立连接=====================
		bool ret = TCP_Connect();
		if (ret == 0) {
			cout << "三次握手:服务器响应失败!" << endl;
			exit(EXIT_FAILURE);
		}

		cout << ">>>>>>>>>>>>>>>>>>>>   Connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		//服务器端首先需要先收到消息，才能判断是进行关闭连接还是文件传输
		//处理是先进行接收消息和检测，若检测到FIN，则中断文件传输跳转到四次挥手

		// 5、服务器端处理消息，根据类型做不同处理
		DATA_PACKAGE NowMessage;
		while (1) {
			int recv = recvfrom(Server_Socket, (char*)&NowMessage, sizeof(NowMessage), 0, (sockaddr*)&Router_Addr, &addr_len);
			if (recv == 0) {
				cout << "数据传输:接收消息失败!" << endl;
			}
			else if (recv > 0) {
				//先看看是不是FIN标志，如果是进入到关闭TCP函数中处理
				if (NowMessage.Udp_Header.flag & FIN) break;
				else if(NowMessage.Udp_Header.flag & FILE_TAG) {
					int ret = Data_Recv(NowMessage);
					if (ret == false) {
						cout << "数据传输:传输过程失败!" << endl;
						exit(EXIT_FAILURE);
					}
				}
			}
		}
		ret = Close_TCP_Connect(NowMessage);
		if (ret == false) {
			cout << "四次挥手:服务器响应失败!" << endl;
			exit(EXIT_FAILURE);
		}

		cout << ">>>>>>>>>>>>>>>>>>>>   Close connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		WSACleanup();
	}
	else {
		cout << "初始化Winsock库失败!" << endl;
		exit(EXIT_FAILURE);
	}
	return 0;
}