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

#define MaxFileSize 200000000 //����ļ���С
#define MaxMsgSize 14000 //�����Ϣ��С
#define MaxWaitTime 2000 //��ʱʱ��
#define MaxSendTime 10 //��ʱ���Դ���

/*IPͷ���壬�Ѽ򻯣������������ݣ���10���ֽ�*/

#pragma pack(1)  //��ֹ�Ż��洢�ṹ���ж��룬��֤�ṹ���ڲ�������������
struct IP_HEADER
{
	unsigned int SrcIp;     //Դip 32λ
	unsigned int DestIp;     //Ŀ��ip 32λ��ʵ������û�д������������ǻ��ص�ַ
	unsigned short TotalLenOfPacket;    //���ݰ����� 16λ��ʵ������û�д������
};
#pragma pack()


/*UDPͷ���壬��20���ֽ�*/

#pragma pack(1)
struct UDP_HEADER
{
	unsigned short SrcPort, DestPort;//Դ�˿ں� 16λ��Ŀ�Ķ˿ں� 16λ
	unsigned int Seq;//���к� 32λ
	unsigned int Ack;//ȷ�Ϻ� 32λ
	unsigned int size;//���ݴ�С 32λ
	char flag;//��־  8λ������λ����
	char empty; //���λ���Ա�֤���룬��ʵ������ 8λ
	unsigned short other; //16λ������
	unsigned short checkNum;//У��� 16λ
};
#pragma pack()

/*���ݰ����壬��20���ֽ�*/

#pragma pack(1)
struct DATA_PACKAGE
{
	IP_HEADER Ip_Header;
	UDP_HEADER Udp_Header;

	//��������,��һ��0��ǽ�β
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
	// ��shortָ�뻮�֣���֤���������ֽ������ֽڵĶ�ȡ
	unsigned short* msg = (unsigned short*)&message;

	// ע�⽫checkNum��Ϊ0���������
	message.Udp_Header.checkNum = 0;
	// �����
	for (int i = 0; i < sizeof(message) / 2; i++) {
		sum += *msg;
		msg++;
		// �ж��Ƿ���ڸ�16λ�����򽫸�λ�ӵ���λ��
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	// ȡ��
	message.Udp_Header.checkNum = ~(sum);
}

bool check(DATA_PACKAGE& message) {
	int sum = 0;
	unsigned short* msg = (unsigned short*)&message;

	for (int i = 0; i < sizeof(message) / 2; i++) {
		sum += *msg;
		msg++;
		// �ж��Ƿ���ڸ�16λ�����򽫸�λ�ӵ���λ��
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