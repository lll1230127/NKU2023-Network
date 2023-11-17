#pragma once
#include <iostream>
using namespace std;

#define FIN 1
#define SYN 2
#define RST 4
#define PSH 8
#define ACK 16
#define URG 32
#define FILE_TAG 64
#define RDT_SEQ 128

#define MaxFileSize 200000000 //最大文件大小
#define MaxMsgSize 14000 //最大信息大小
#define MaxWaitTime 2000 //超时时限
#define MaxSendTime 10 //超时重试次数

/*IP头定义，已简化，仅含必须数据，共10个字节*/

#pragma pack(1)  //禁止优化存储结构进行对齐，保证结构体内部数据连续排列
struct IP_HEADER
{
	unsigned int SrcIp;     //源ip 32位
	unsigned int DestIp;     //目的ip 32位，实际上我没有处理这俩，都是环回地址
	unsigned short TotalLenOfPacket;    //数据包长度 16位，实际上我没有处理这个
};
#pragma pack()


/*UDP头定义，共20个字节*/

#pragma pack(1)
struct UDP_HEADER
{
	unsigned short SrcPort, DestPort;//源端口号 16位、目的端口号 16位
	unsigned int Seq;//序列号 32位
	unsigned int Ack;//确认号 32位
	unsigned int size;//数据大小 32位
	char flag;//标志  8位，有两位保留
	char empty; //填充位，以保证对齐，无实际意义 8位
	unsigned short other; //16位，计数
	unsigned short checkNum;//校验和 16位
};
#pragma pack()

/*数据包定义，共20个字节*/

#pragma pack(1)
struct DATA_PACKAGE
{
	IP_HEADER Ip_Header;
	UDP_HEADER Udp_Header;

	//报文数据,多一个0标记结尾
	char Data[MaxMsgSize];

	DATA_PACKAGE() {
		memset(&Ip_Header, 0, sizeof(Ip_Header));
		memset(&Udp_Header, 0, sizeof(Udp_Header));
		memset(&Data, 0, sizeof(Data));
	};
};
#pragma pack()

void set_checkNum(DATA_PACKAGE& message) {
	int sum = 0;
	// 用short指针划分，保证按照两个字节两个字节的读取
	unsigned short* msg = (unsigned short*)&message;

	// 注意将checkNum置为0，方便求和
	message.Udp_Header.checkNum = 0;
	// 计算和
	for (int i = 0; i < sizeof(message) / 2; i++) {
		sum += *msg;
		msg++;
		// 判断是否存在高16位，否则将高位加到低位上
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	// 取反
	message.Udp_Header.checkNum = ~(sum);
}

bool check(DATA_PACKAGE& message) {
	int sum = 0;
	unsigned short* msg = (unsigned short*)&message;

	for (int i = 0; i < sizeof(message) / 2; i++) {
		sum += *msg;
		msg++;
		// 判断是否存在高16位，否则将高位加到低位上
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}

	if ((sum & 0xFFFF) == 0xFFFF) {
		return true;
	}
	return false;
}

int get_RdtSeq(DATA_PACKAGE& message) {
	return ((message.Udp_Header.flag & 0x80) >> 7);
}

int get_RdtAck(DATA_PACKAGE& message) {
	return !get_RdtSeq(message);
}