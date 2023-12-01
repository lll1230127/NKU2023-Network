#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string.h>
#include<fstream>
#include <mutex>
#include "Message.h"

// 地址：C:\Users\34288\Desktop\pic\1.jpg

// lab1中提到，防止函数报错
#pragma comment (lib, "ws2_32.lib")
std::mutex mtx; // 创建输出的互斥锁

using namespace std;

// 设置路由器和客户端的端口号
const int RouterPORT = 23381; //路由器端口号
const int ClientPORT = 23380; //客户端端口号

// 设置用户socket和路由器socket
SOCKET Router_Socket;
SOCKET Client_Socket;

SOCKADDR_IN Router_Addr;
SOCKADDR_IN Client_Addr;

// 全局变量
int addr_len = sizeof(Router_Addr);   //地址长度
int now_seq = 0;    //当前的tcp seq(发送)
int now_ack = 0;    //当前的tcp ack(接收)

// 多线程所需的全局变量
int recv_mark = -1;  //返回标记
int wait[MaxWindowSize] = {0};  //停等标记
int	Fin_Trans = 1;	//传输结束标识


// 滑动窗口所需的全局变量
// 接收到的GBN序列号和发送的GBN序列号
int recv_GBN = 0;
int send_GBN = 0;
// 滑动窗口基址和下一个要发送的序号
int window_base = 0;
int next_send = 0;
// 发送缓冲区
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
			//检查有没有成功发送
			clock_t message_start = clock();
			if (send == 0) {
				cout << "数据传输：发送失败!" << endl;
				exit(EXIT_FAILURE);
			}
		}
	}
}

DWORD WINAPI GetAckThread() {
	DATA_PACKAGE ACK_message;
	//建立新线程来接听，主线程用于超时重传的计时和信息处理，
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &ACK_message, 0, 0);

	// 开始的时候设置时钟，用于接收的超时重传
	clock_t message_start = clock();

	// 设置计数，用于接收的快速重传
	int counter = 0;

	//检验和超时重传
	recv_mark = -1;
	while (1) {
		//不为负1的时候表明收到了消息
		if (window_base == next_send && Fin_Trans == 0)break;
		if (recv_mark == 0) return false;
		//成功收到消息
		else if (recv_mark > 0) {
			//进行处理
			mtx.lock();
			cout << "【接收ACK播报】客户端成功收到" << int(ACK_message.Udp_Header.other) << "号数据包的ACK！";
			cout << "其中：GBN序列号为:" << int(ACK_message.Udp_Header.GBN) <<",待确认的GBN序列号为" << (recv_GBN+1) % MaxWindowSize<<endl;
			//收到未确认ACK，可能存在较大的情况
			if ((ACK_message.Udp_Header.flag & ACK) && ((ACK_message.Udp_Header.GBN - recv_GBN + MaxWindowSize) % MaxWindowSize) >= 1) {
				cout << "校验和为(对为1错为0):" << check(ACK_message) << ",信息准确,确认"<< int(ACK_message.Udp_Header.other)<<"号数据包已经接收！" << endl<<endl;
				mtx.unlock();
				if (!check(ACK_message)) break;
				while (recv_GBN != ACK_message.Udp_Header.GBN) {
					wait[recv_GBN] = 0;
					recv_GBN = (recv_GBN + 1) % MaxWindowSize;
				}
				counter = 0;
				if (window_base == next_send && Fin_Trans == 0)break;
				//收到新的ack，重新开始计时！
				message_start = clock();
				mtx.lock();
				cout << "【窗口状态信息播报】 ";
				cout << "窗口基址:" << window_base << " 即将发送的数据包为:" << next_send + 1;
				cout << " 已发送未确认共有:" << next_send - window_base << " 可用待发送共有:" << MaxWindowSize - (next_send - window_base) << endl << endl;
				mtx.unlock();
			}
			//收到已确认ACK
			else if ((ACK_message.Udp_Header.flag & ACK) && (recv_GBN % MaxWindowSize) == ACK_message.Udp_Header.GBN) {
				if (!check(ACK_message)) break;
				counter++;
				cout<< "校验和为(对为1错为0):" << check(ACK_message) << ",信息准确," << "但是为已确认数据包！目前已收到" << counter << "个" << endl <<endl;
				if (counter >= 3) {
					cout << ">>>>>>>>准备快速重传！重传"<< next_send -window_base<<"个数据包<<<<<<<<<" << endl<<endl;
					mtx.unlock();
					Send_Again(recv_GBN, next_send - window_base);
					counter = 0;
				}
				else {
					mtx.unlock();
				}
			}
			//重新接收
			recv_mark = -1;
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &ACK_message, 0, 0);
		}
		//超时，准备重传
		if (clock() - message_start > MaxWaitTime)
		{
			cout << ">>>>>>>>发送超时，准备重传全部信息......<<<<<<<<<" << endl<<endl;
			Send_Again(recv_GBN, MaxWindowSize);
			message_start = clock();
		}
	}
	recv_mark = -1;
	return 0;
}


int SendMyMessage(DATA_PACKAGE& message) {

	int send = sendto(Client_Socket, (char*)&message, sizeof(message), 0, (sockaddr*)&Router_Addr, addr_len);
	//检查有没有成功发送
	if (send == 0) {
		cout << "数据传输：发送失败!" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		mtx.lock();
		cout << "【数据发送播报】客户端成功发送"<< int((message).Udp_Header.other)<<"号数据包！" << endl;
		cout << "其中：GBN序列号为:" << int((message).Udp_Header.GBN) << ",已确认的GBN序列号为" << (recv_GBN) % MaxWindowSize << endl << endl;
		mtx.unlock();
		next_send++;
		mtx.lock();
		cout << "【窗口状态信息播报】";
		if((window_base == next_send)&& (window_base == 0)) cout << "传输已经结束！此时窗口右移到最后，已重置信息";
		else cout << "窗口基址:" << window_base << " 即将发送的数据包为:" << next_send + 1;
		cout << " 已发送未确认共有:" << next_send - window_base << " 可用待发送共有:" << MaxWindowSize - (next_send - window_base)<< endl << endl;
		mtx.unlock();
	
	}
}

bool Close_TCP_Connect(){
	DATA_PACKAGE message1;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;
	DATA_PACKAGE message4;

	//-----------第一次挥手处理开始（FIN=1，seq=u）-----------
	message1.Udp_Header.SrcPort = ClientPORT;
	message1.Udp_Header.DestPort = RouterPORT;
	message1.Udp_Header.flag += FIN;//设置FIN
	message1.Udp_Header.Seq = now_seq++;//设置序号seq
	message1.Udp_Header.Ack = now_ack;

	set_checkNum(message1);
	int send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
	
	// 设置时钟，用于接收的超时重传
	clock_t message1_start = clock();

	//检查有没有成功发送
	if (send1 == 0) return false;
	cout << "客户端第一次挥手成功发送！" << endl;

	//-----------第二次挥手处理开始（ACK=1，seq =v ，ack=u+1）-----------
	//建立新线程，用于监听有无收到第二次握手，主线程用于超时重传处理
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message2, 0, 0);

	//检验和超时重传
	while (1) {
		//不为负1的时候表明收到了消息
		if (recv_mark == 0) return false;
		//成功收到消息
		else if (recv_mark > 0) {
			//检查标记位和序列号ack，保证可靠性传输
			cout << "客户端第二次挥手成功收到！";
			if ((message2.Udp_Header.flag & ACK) && (message2.Udp_Header.Ack == message1.Udp_Header.Seq + 1)) {
				if (!check(message2)) return false;
				cout << "且信息准确无误" << endl;
				break;
			}
			else return false;
		}
		//第一次握手的message1超时，准备重传
		if (clock() - message1_start > MaxWaitTime)
		{
			cout << "第一次挥手超时，准备重传......" << endl;
			send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
			message1_start = clock();
			if (send1 == 0) return false;
		}
	}
	recv_mark = -1;

	//-----------第三次挥手处理开始（FIN=1，ACK=1，seq=w，ack=u+1）-----------
	recv_mark = recvfrom(Client_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, &addr_len);
	//不为负1的时候表明收到了消息
	if (recv_mark == 0) return false;
	//成功收到消息
	else if (recv_mark > 0) {
		//检查标记位和序列号ack，保证可靠性传输
		cout << "客户端第三次挥手成功收到！";
		if ((message3.Udp_Header.flag & ACK) && (message3.Udp_Header.flag & FIN)&& (message3.Udp_Header.Ack == message1.Udp_Header.Seq + 1)) {
			if (!check(message3)) return false;
			cout << "且信息准确无误" << endl;
		}
		else return false;
	}
	recv_mark = -1;

	//-----------第四次挥手处理开始（ACK=1，seq=u+1，ack = w+1）-----------
	message4.Udp_Header.SrcPort = ClientPORT;
	message4.Udp_Header.DestPort = RouterPORT;
	message4.Udp_Header.flag += ACK;//设置ACK
	message4.Udp_Header.Seq = now_seq;//设置序号seq=u+1;
	message4.Udp_Header.Ack = message3.Udp_Header.Seq + 1;
	now_ack = message4.Udp_Header.Ack;
	set_checkNum(message4);//设置校验和
	int send4 = sendto(Client_Socket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&Router_Addr, addr_len);
	if (send4 == 0)
	{
		return false;
	}
	cout << "客户端第四次挥手成功发送！" << endl;

	//-----------等待2MSL，防止ACK丢失-----------
	int close_clock = clock();
	cout << "客户端进入2MSL等待..." << endl;
	//若收到提示信息，重新发送
	DATA_PACKAGE tmp;
	
	//创建最后的接收线程线程
	HANDLE wait = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &tmp, 0, 0);

	while (clock() - close_clock < 2 * MaxWaitTime)
	{
		if (recv_mark == 0) return false;
		else if (recv_mark > 0)
		{
			//丢失的ack
			send4 = sendto(Client_Socket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&Router_Addr, addr_len);
			cout << "异常恢复：重新发送结束ACK" << endl;
		}
	}

	//回收线程
	TerminateThread(wait, EXIT_FAILURE);
	cout << "客户端关闭连接成功！此时TCP序列号、确认号为" << now_seq << " " << now_ack << endl;
}

bool TCP_Connect(){
	DATA_PACKAGE message1;
	DATA_PACKAGE message2;
	DATA_PACKAGE message3;
	//-----------第一次握手处理开始（SYN=1，seq=x）-----------
	//实际上应该随机分配一个seq，这里简化了
	message1.Udp_Header.SrcPort = ClientPORT;
	message1.Udp_Header.DestPort = RouterPORT;
	message1.Udp_Header.flag += SYN;//设置SYN
	message1.Udp_Header.Seq = now_seq++;//设置序号seq
	set_checkNum(message1);//设置校验和

	int send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
	
	// 设置时钟，用于接收的超时重传
	clock_t message1_start = clock();

	//检查有没有成功发送
	if (send1 == 0) return false;
	cout << "客户端第一次握手成功发送！" << endl;

	//-----------第二次握手处理开始（SYN=1，ACK=1，ack=x）-----------
	//建立新线程，用于监听有无收到第二次握手，主线程用于超时重传处理
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &message2, 0, 0);

	//检验和超时重传
	while (1){
		//不为负1的时候表明收到了消息
		if (recv_mark == 0) return false;
		//成功收到消息
		else if (recv_mark > 0){
			//检查标记位和序列号ack，保证可靠性传输
			cout << "客户端第二次握手成功收到！";
			if ((message2.Udp_Header.flag & ACK) && (message2.Udp_Header.flag & SYN)  && (message2.Udp_Header.Ack == message1.Udp_Header.Seq+1)){
				if (!check(message2)) return false;
				cout << "且信息准确无误" << endl;
				break;
			}
			else return false;
		}
		//第一次握手的message1超时，准备重传
		if (clock() - message1_start > MaxWaitTime)
		{
			cout << "第一次握手超时，准备重传......" << endl;
			send1 = sendto(Client_Socket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&Router_Addr, addr_len);
			message1_start = clock();
			if (send1 == 0) return false;
		}
	}
	recv_mark = -1;

	//-----------第三次握手处理开始（ACK=1，seq=x+1）-----------
	message3.Udp_Header.SrcPort = ClientPORT;
	message3.Udp_Header.DestPort = RouterPORT;
	message3.Udp_Header.flag += ACK;//设置ACK
	message3.Udp_Header.Seq = now_seq;//设置序号seq=x+1
	message3.Udp_Header.Ack = message2.Udp_Header.Seq + 1;
	now_ack = message3.Udp_Header.Ack;
	set_checkNum(message3);//设置校验和
	int send3 = sendto(Client_Socket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&Router_Addr, addr_len);
	if (send3 == 0)
	{
		return false;
	}
	cout << "客户端第三次挥手成功发送！" << endl;
	cout << "客户端连接成功！此时TCP序列号、确认号为" <<now_seq<<" "<<now_ack << endl;
	return true;
}

bool Data_Send(string filename) {
	int rdt_seq = 0; //记录当前rdt_seq

	//1、处理文件路径，获取文件名
	string realname = "";
	for (int i = filename.size() - 1; i >= 0; i--)
	{
		if (filename[i] == '/' || filename[i] == '\\') break;
		realname += filename[i];
	}
	realname = string(realname.rbegin(), realname.rend());

	//2、打开文件，将文件转为字节流并写入filemessage
	ifstream fin(filename.c_str(), ifstream::binary);
	if (!fin) {
		printf("无法打开文件！\n");
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

	//3、发送预告消息：告知接收端文件名和文件信息，以及窗口大小
	DATA_PACKAGE nameMessage;
	nameMessage.Udp_Header.SrcPort = ClientPORT;
	nameMessage.Udp_Header.DestPort = RouterPORT;
	//当flag出现FILE，size位意义改变，从数据包大小变为整个文件大小
	nameMessage.Udp_Header.size = file_size;
	nameMessage.Udp_Header.flag += FILE_TAG;
	nameMessage.Udp_Header.Seq = now_seq;
	nameMessage.Udp_Header.Ack = now_ack;
	nameMessage.Udp_Header.GBN = MaxWindowSize;
	nameMessage.Udp_Header.other = 0;

	for (int i = 0; i < realname.size(); i++)//填充报文数据段
		nameMessage.Data[i] = realname[i];
	nameMessage.Data[realname.size()] = '\0';//字符串结尾补\0


	//完成数据包的构成后不能忘了设置校验和
	set_checkNum(nameMessage);
	int send0 = sendto(Client_Socket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&Router_Addr, addr_len);
	Data_message[0] = nameMessage;
	wait[0] = 1;

	// 设置时钟，用于接收的超时重传
	clock_t message0_start = clock();

	//检查有没有成功发送
	if (send0 == 0) return false;
	cout << "客户端成功发送0号数据包(文件名和文件信息)！" << endl<<endl;

	//超时检测：检验服务器端有没有收到，代码写到这里我不禁十分愤怒，早知道把发送和超时检测封装一下了
	//写完这个我马上就去封装，忍不了了。

	DATA_PACKAGE recv_message;
	//建立新线程，用于监听有无收到ACK，主线程用于超时重传处理
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &recv_message, 0, 0);

	while (1) {
		//不为负1的时候表明收到了消息
		if (recv_mark == 0) return false;
		//成功收到消息
		else if (recv_mark > 0) {
			//检查标记位和序列号ack，保证可靠性传输
			cout << "客户端成功收到"<<int(recv_message.Udp_Header.other)<<"号数据包的ACK！"<<endl<<endl;
			if ((recv_message.Udp_Header.flag & ACK)) {
				if (!check(recv_message)) return false;
#ifdef test
				cout << "且信息准确无误" << endl<<"其中：RDT确认号为:" << get_RdtAck(recv_message) << ",校验和为(对为1错为0):" << check(recv_message);
				cout << ",TCP序列号为:" << recv_message.Udp_Header.Seq << ",TCP确认号为:" << recv_message.Udp_Header.Ack << endl<<endl;
#else 
				cout << endl;
#endif 
				wait[0] = 0;
				break;
			}
			else return false;
		}
		//message超时，准备重传
		if (clock() - message0_start > MaxWaitTime)
		{
			cout << "0号数据包超时，准备重传......" << endl;
			send0 = sendto(Client_Socket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&Router_Addr, addr_len);
			message0_start = clock();
			if (send0 == 0) return false;
		}
	}
	recv_mark = -1;
	Fin_Trans = 1;

	//4、接下来，如果服务器端同意（实际上没做检验），就可以发送全部的数据了，分组进行，之后会在这部分优化为滑动窗口
	//为了为之后做准备，这里也用多线程实现，实际上停等机制可以不建新线程。

	int package_num = file_size / MaxMsgSize;//全装满的报文个数
	int left_size = file_size % MaxMsgSize;//不能装满的剩余报文大小

	// 开始计算传输的时间
	int start_time = clock(); //计算吞吐率

	// 创建唯一的消息接收线程
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GetAckThread,0, 0, 0);


	//依次建立发送，监听回复全部在相应线程实现
	for (int i = 0; i < package_num; i++) {
		//等待接收，即滑动窗口的等待实现
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

		//lab3-2新增，GBN序列号的维护
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

	//剩余部分
	if (left_size > 0)
	{
		//等待接收，即滑动窗口的等待实现
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

		//lab3-2新增，GBN序列号的维护
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

	//等待全部接收完毕,重置参数！
	while (Fin_Trans) {
		if (get_wait() >= 0) {
			window_base++;
			send_GBN = (send_GBN + 1) % MaxWindowSize;
			if (window_base == next_send) {
				Fin_Trans = 0;
				recv_GBN = 0;
				send_GBN = 0;
				// 滑动窗口基址和下一个要发送的序号
				window_base = 0;
				next_send = 0;
				break;
			}
		}
	}

	//计算传输时间和吞吐率
	int end_time = clock();
	cout << ">>>>>>>>>>>>>>>>>>>>   Data transmission successfully  <<<<<<<<<<<<<<<<<<<<" << endl;
	cout << "总体传输时间为:" << (end_time - start_time) / CLOCKS_PER_SEC << "s" << endl;
	cout << "吞吐率:" << ((float)file_size) / ((end_time - start_time) / CLOCKS_PER_SEC) << "byte/s" << endl << endl;

	return true;
}

int main()
{
	// 按照类似于lab1的步骤初始化客户端环境

	// 1、初始化Winsock库
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);

	if (res == 0) {
		cout << "成功初始化Winsock库!" << endl;

		// 2、创建用户端的socket并绑定地址
		Client_Socket = socket(AF_INET, SOCK_DGRAM, 0);
		Client_Addr.sin_family = AF_INET;
		// 端口号设置
		Client_Addr.sin_port = htons(ClientPORT);
		// ip地址设置为我们本机的ip地址
		inet_pton(AF_INET, "127.0.0.1", &Client_Addr.sin_addr.S_un.S_addr);
		bind(Client_Socket, (LPSOCKADDR)&Client_Addr, sizeof(Client_Addr));

		// 返回为INVALID_SOCKET时候，说明初始化失败，进行异常处理
		if (Client_Socket == INVALID_SOCKET) {
			cout << "创建用户端Socket失败!" << endl;
			exit(EXIT_FAILURE);
		}
		else cout << "成功创建用户端Socket!" << endl;

		// 3、绑定路由器的ip地址和进程端口号
		Router_Addr.sin_family = AF_INET;
		// 端口号设置
		Router_Addr.sin_port = htons(RouterPORT);
		// ip地址设置为我们本机的ip地址
		inet_pton(AF_INET, "127.0.0.1", &Router_Addr.sin_addr.S_un.S_addr);

		cout << ">>>>>>>>>>>>>>>>>>>>   Client get ready  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		// 4、向服务器发出请求（这里我们实现了TCP三次握手的模拟）
		bool ret = TCP_Connect();
		if (ret == false) {
			cout << "三次握手:连接服务器失败!" << endl;
			exit(EXIT_FAILURE);
		}
		
		cout << ">>>>>>>>>>>>>>>>>>>>   Connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		// 5、接收用户指令来选择传输或者中断
		while (1)
		{
			int choice;
			cout << "请输入您的操作：" << endl << "（终止连接——1		传输文件——0）" << endl;
			cin >> choice;
			if (choice == 1)break;
			else if (choice == 0) {
				string filename;
				cout << "请输入文件路径：" << endl;
				cin >> filename;
				if (!Data_Send(filename)) {
					cout << "数据传输出错！请重试" << endl << endl;
				}
			}
		}

		// 6、进行四次挥手断开连接
		ret = Close_TCP_Connect();
		if (ret == false) {
			cout << "四次挥手:断开服务器失败!" << endl;
			exit(EXIT_FAILURE);
		}

		cout << ">>>>>>>>>>>>>>>>>>>>   Close connect successfully  <<<<<<<<<<<<<<<<<<<<" << endl << endl;

		closesocket(Client_Socket);
		WSACleanup();
	}
	else {
		cout << "初始化Winsock库失败!" << endl;
		exit(EXIT_FAILURE);
	}
}