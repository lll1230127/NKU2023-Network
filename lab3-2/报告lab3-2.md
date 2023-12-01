## 计算机网络实验报告

### Lab3-2 **可靠数据传输代码实现**-基于GBN的累计确认

#### 学号：2112338     姓名：李威远

[TOC]

### 一、协议设计：

#### （一）实验要求

> **实验要求：**本次实验中，我们需要基于UDP数据报套接字，运用socket编程，在上一次lab3-1的实验基础上，将原本的停等机制改进成**基于滑动窗口的流量控制机制**，采用固定窗口大小，支持**累积确认**，完成给定测试文件的传输
>
> ➢ 协议设计：数据包格式，发送端和接收端交互，详细完整
>
> ➢ 流水线协议：多个序列号
>
> ➢ 发送缓冲区、接收缓冲区
>
> ➢ 累积确认：**Go Back N**
>
> ➢ 日志输出：收到/发送数据包的序号、ACK、校验和等，发送端和接收端的
>
> 窗口大小等情况，传输时间与吞吐率
>
> ➢ 测试文件：必须使用助教发的测试文件（1.jpg、2.jpg、3.jpg、helloworld.txt）
>
> ​	整个程序的具体代码实现已经上传到我的[github]([lll1230127/NKU2023-Network (github.com)](https://github.com/lll1230127/NKU2023-Network))当中。
>

#### （二）设计思路

> **写在前面：**协议具体的设计思路，这里只介绍lab3-2新增的，lab3-1和lab3-2整个的协议设计会在之后的协议展示给出。

##### **从GBN协议设计思考**

​	首先需要回顾课程中所学的Go-Back-N(GBN)协议，看看在我们先前实现的协议设计基础上，我们还需要增加什么信息。这里，我们不展开介绍GBN协议的具体实现原理和滑动窗口相关的机制，只分析我们所需增加的协议组成。

​	GBN协议相较于Rdt3.0，最大的变化就是引入了**流水线机制，来提高资源的利用率**

![1](pic\1.jpg)

​	可以看到，在GBN协议中，我们会连续发送多个协议，这是基于滑动窗口机制来实现的，我们这里需要思考的是，一次发送多个协议的情况下，我们先前实现的协议能够满足这个需求吗？

​	我们在lab3-1中实现了UDP报文结构，并基于rdt协议和TCP三次握手和四次挥手的需要，分别增加了TCP相关的seq和rdt协议相关的seq。在基于GBN的滑动窗口的数据传输过程中，我们同样也需要维护一个seq来保证传输的有序。

​	显然，**原先1位的rdt协议相关的seq已经不足以满足我们的需求**。此外，为了提高代码简洁性和实现性，我们不再维护TCP的序列号和确认号，因为实验的核心是基于GBN协议来实现我们所需的累计确认。

​	对于GBN协议所需维护的seq，需要有几位呢？类似于rdt协议，这里最少需要**滑动窗口大小的seq**来表示，也就是连续发送的协议的数量。

​	综上，我对协议所作的更改实际上就是删去我们先前的seq，增加一个GBN协议对应的Seq序列号即可，为了代码规整性，我们不再改动之前的协议，只增加一个GBN序列号。

#### （三）协议展示

> **写在前面：**这部分协议展示包含了上一次实验中的协议设计和我们增加的内容，增加的内容用红色字体着重突出了。

##### 1、消息类型

​	在我们的协议设计中，不同的消息类型实际上是由标记位决定的，其内部的结构是统一的，作用会根据标记位的不同产生不同。结合三次握手和四次挥手、数据传输的整个过程，我们按照标记区分不同的消息类型，总共分为以下几种：

- **连接请求消息（SYN）：**由客户端向服务器端发送，即三次握手的第一次握手的消息，表明希望建立一个连接。
- **连接同意消息（SYN、ACK）：**由服务器向客户端发送，即三次握手的第二次握手的消息，表明同意建立这个连接。
- **断开请求信息（FIN）：**由客户端向服务器端发送，即四次挥手的第一次挥手的消息，表明希望断开连接。
- **断开同意信息（FIN、ACK）：**由服务器向客户端发送，即四次挥手的第三次挥手的消息，表明同意断开这个连接。
- **数据传输预告信息（FILE_TAG）:**由客户端向服务器端发送，即数据传输开始的消息，表明开始数据的传输。
- **数据传输信息（无）：**由客户端向服务器端发送，即数据传的数据报文，其中包含文件的数据信息。<font color='red'>这里，每一个文件都包含一个GBN的序列号，用于累计确认工作。</font>
- **确认消息（ACK）：**双方都可能发送，用于确认对方的上一条数据报已经成功收到，且结果准确无误。<font color='red'>其中，如果接收端收到非期待的数据报，会返回上一次确认接收的数据报（即期待序列号减一的数据报）的ACK，以进行快速重传，提高传输速率。</font>

##### 2、报文结构（语法及其语义）

​	基于以上设计，我们给出了如下的报文结构设计，所有的消息类型的结构都如下所示，但其标记位（flags）不同，功能也不同。

​	主要是UDP头的设计，这是**整个报文结构设计的核心**，我们重点讲一下这部分：

```c++
#pragma pack(1)
struct UDP_HEADER
{
	unsigned short SrcPort, DestPort;//源端口号 16位、目的端口号 16位
	unsigned int Seq;//序列号 32位
	unsigned int Ack;//确认号 32位
	unsigned int size;//数据大小 32位
	char flag;//标志  8位
	char GBN; //GBN序列号，用于滑动窗口
	unsigned short other; //16位，计数
	unsigned short checkNum;//校验和 16位
};
#pragma pack()
```

​	其中，各字段的功能（语义）在先前的lab3-1中大部分都已经提到，这里简单介绍一下这些自段，并重点说明一下增加的GBN字段。

- **伪头部 ：** 只是为了提取 IP 数据报中的源IP，目的IP信息并加上协议等字段构造的数据。在实际传输中并不会发送，仅起到校验和计算使用，因此称之为伪首部。
- **源端口号 :** 一般是客户端程序请求时,由系统自动指定,端口号范围是 0 ~ 65535，0~ 1023为知名端口号。
- **目的端口 ：** 一般是服务器的端口，一般是由编写程序的程序员自己指定，这样客户端才能根据ip地址和 port 成功访问服务器
- **UDP长度 ：**是指整个UDP数据报的长度 ，包括 报头 + 载荷
- **UDP校验和 ：**用于检查数据在传输中是否出错，是否出现bit反转的问题，当进行校验时，需要在UDP数据报之前增加临时的伪首部。
- **flag标志：**包含我们先前提到的各个信息的标志，用于标记该数据报文是什么信息。

- **GBN：**GBN字段其实就是原本的empty填充位，当时为了保证我们的报文结构是16位对齐，便于进行校验和的计算，所以增加了这一段作为填充位。<font color='red'>这里，我利用这8位的GBN字段来存储GBN的序列号，也就是一个不超过MaxWindowSize的相对序列号。</font>

  <font color='red'>只使用不超过MaxWindowSize的相对序列号类似于我们在rdt3.0中实现的单比特序列号，同样能够占用最小空间地完成我们累计重传的工作。</font>

​	接下来，我们来看看最后的封装，也就是我们的数据包结构，它封装了我们刚刚给出的IP头和UDP头，以及一个char类型的报文数据，其中宏MaxMsgSize是指最大报文数据长度，用于后续的文件数据分组传输。

​	这里，我们对先前给出的字段内容都初始化为0。

```c++
#pragma pack(1)
struct DATA_PACKAGE
{
	IP_HEADER Ip_Header;
	UDP_HEADER Udp_Header;

	//报文数据
	char Data[MaxMsgSize];

	DATA_PACKAGE() {
		memset(&Ip_Header, 0, sizeof(Ip_Header));
		memset(&Udp_Header, 0, sizeof(Udp_Header));
		memset(&Data, 0, sizeof(Data));
	};
};
#pragma pack()
```

##### 3、时序

​	对于三次握手和四次挥手的时序，可以参考我们上次实验中的三次握手和四次挥手的时序逻辑关系图，如三次挥手的过程如下，都已经在上次实验中详细介绍，不多赘述：

![3](pic\3.jpg)

​	这里需要注意不同的是，由于我们是基于UDP数据报套接字，所以**上图中的服务器进入监听状态实质上在我们的实现中是不存在的**，客户端和服务器端都是如我们接下来展示的功能设计原理中的UDP套接字连接过程来建立连接，没有出现监听状态。

​	如下是四次挥手的时序图，也已经在上次实验中详细介绍，这里同样不过多解释：

![4](pic\4.jpg)

​	需要注意的是，<font color='red'>不同于lab3-1，本次实验中，服务器端需要向客户端回复ACK，且不是停等协议，发生服务器端已经接收完全部数据，但客户端还没有收到最后的ACK的现象的可能性大大上升，这要求我们必须重视这个事实，作出相应的处理。</font>

​	我们有两种解决方式，分别是：

- <font color='red'>结合多线程和四次挥手的第二次挥手的等待，等待数据传输彻底完成后再彻底挥手。</font>并在服务器结束接收后增加等待时间，看客户端是否继续传输数据，若继续传输说明ACK未收到，需要重传ACK，这需要额外运用一些多线程编程，也会增加一些传输时间上的消耗。
- <font color='red'>修改代码的处理逻辑，在服务器结束接收后，继续对新传入的数据报进行检查。</font>如果新数据报为数据传输信息，表明客户端还在进行数据传输，需要进行重传ACK处理，确保客户端能够正确接收到最后的ACK。

​	这里，我们选择了第二种处理方式，从根本上避免了这种情况的产生。

​	最后，如下图所示是建立连接后的数据传输的时序图，根据GBN协议设计形成，但是迫于时序图设计逻辑，没有在图中加上超时重传的处理，但代码中肯定是实现了的：

​	![4](pic\6.png)

​	从上图可以看出，实际上我们实现的GBN的滑动窗口传输代码流程是基于课程中所学的GBN状态机来设计的，接下来，我们在原理部分详细回忆GBN协议的内容，并在代码实现部分阐述我们是如何实现的GBN协议功能。

### 二、功能设计与原理

> **写在前面**：功能设计部分旨在对上部分的协议设计进行补充，前者着重于核心数据结构的实现，而后者着重于具体代码的实现。

#### （一）GBN协议内容总览

​	本次实验在上一次实验rdt协议的停等机制基础上，给发送端增加了缓存区，基于Go-Back-N（GBN）协议，实现了有**滑动窗口、进行累计确认**的数据传输。并基于此为了提高数据传输速率，增加了**快速重传**的相关处理。

​	首先，先来宏观的回顾GBN协议的内容。从名称上来看，**Go-Back-N（GBN）协议即指发送错误后，需要退回到最后正确连续帧的位置开始重发**，因此其侧重点实际上在于错误处理的重传机制的不同。

​	**对比ARQ来看，GBN引入了滑动窗口，实现了流水线，大大提升了传输效率。**这是由于ARQ在每一次发送报文时，都需要等待上个报文的确认报文被接收到，才可以继续发送，因此会产生很长的等待时延，效率低下。而流水线协议则指的是在确认未返回之前允许发送多个分组，这需要依托于**滑动窗口的维护、准确的确认机制、可靠的错误处理**。

​	上述这三部分是我们代码实现的三个核心部分，我们后续分成三块介绍这三部分的设计，也是GBN发送端接收端状态机中所想要维护实现的部分，我们可以回顾一下课程中所介绍的GBN传输的状态机：

![2](pic\2.jpg)

​	可以看到，发送端**除去出错态和起始态**之外，主要维护了base、nextseqnum，sndpkt以及计时器timer四个变量，其中前两者初始为1，分为以下三个状态：

- **发送态：**在发送态中，我们将要发送的数据加入到sndpkt这个缓冲区中。我们每发送一个数据，就会增大一下nextseqnum，这实际上实现了<font color='red'>滑动窗口的空闲块的利用</font>，如果是起始状态，还需要打开计时器，这实际上是在完成<font color='red'>起始数据报的超时重传</font>。
- **接收态：**在接收态中，我们每接收并确认（<font color='red'>这里确认的过程就是累计确认的实现</font>）一个数据，就会增大一下base，实际上这实现了<font color='red'>滑动窗口的右移</font>。此外，如果是结束状态，关掉计时器，如果不是，重新开始计时，<font color='red'>这是在对新的发送开始超时重传</font>。
- **超时态**：当计时器超时，说明我们需要进行重传处理，这里，我们将缓冲区中的所有base到nextseqnum区间的报文都进行重传，实现我们的Go-Back-N操作。

![5](pic\5.jpg)

​	可以看到，接收端需要做的处理更少，除了初始态外，只有接收正确态和其他态两个状态，只在接收正确态进行expectedseqnum和sndpkt进行维护，其中sndpkt不同于发送端，大小仅为1个数据报文，也就是对应着<font color='red'>GBN的发送端滑动窗口大小>1，而接收端窗口大小=1这一事实。</font>

- **接收正确态：**若为期待的数据报文，进入该状态，使用sndpkt暂存该报文的ACK，并返回这个ACK，对expectedseqnum增加，这样<font color='red'>顺次增加期待报文，不会接收除了期待报文之外的报文的处理，保障了累计重传的顺利实现</font>。
- **其他态：**当报文不符合期待时，对其直接丢弃，这里，我们如果不实现快速重传，重发的ACK实际上没有意义，我们放到后面再分析。

#### （二）滑动窗口的维护设计

![7](pic\7.jpg)

​	类似于状态机中给出的base和nextseqnum，我们设置了宏MaxWindowSize，维护了window_base和next_send两个全局变量指针，在发送和接收的时候对其进行调整：

- **MaxWindowSize：**窗口大小，当next_send-window_base小于MaxWindowSize的时候，说明窗口还有可用区域，可以继续发送，否则需要等待继续确认。
- **window_base：**滑动窗口基地址的序号，也就是**滑动窗口的起始地址**，我们使其每次都指向发送端已经确认的最大的序号。
- **next_send:**  下一个准备发送的序号，实际上是**滑动窗口的结束地址**，我们使其每次都指向即将要发送的序号，在结束时使其指向最后一个序号。

​	具体的维护上，按照以下步骤进行滑动窗口的维护，从而实现数据传输中的流量控制：

- 开始传输时，window_base和next_send都指向第一个序号。
- 当**窗口还有空闲位时（如一开始），发送端的主线程发送报文**，每发送一个，右移next_send指针，直至next_send-window_base等于MaxWindowSize，表明窗口已经没有可以发送的空位了。
- 当发送端的子线程接收到确认报文并成功确认，整个窗口通过给window_base指针右移到累计确认的序号处，实现窗口的右移，从而**窗口空闲位增加，能够继续发送**。
- 当发生错误的时候，如发送端需要进行重传，此时window_base和next_send不变，即窗口不懂，只需要重传窗口缓冲区即可，我们在第四部分的重传机制中介绍。

#### （三）准确的确认机制：累计确认

​	GBN协议中，我们采用累计确认来进行确认，回顾累计确认的官方定义：**对某一数据帧的确认，就表明该数据帧和此前所有的数据帧均已正确无误地收到，这种确认方式称为累积确认**。

​	对数据帧有没有收到的确认**发生在发送端**，但是为了实现累计确认这一机制，需要我们在接收端进行处理，保证我们在发送端收到ACK能够进行可靠的累积确认。

​	类似于先前接收端的FSM中给出，我们维护了now_GBN这个全局变量：

- **now_GBN:**  和FSM中的expectedseqnumy类似，即期待接收的序列号。

​	需要注意的是，我们发送的<font color='red'>GBN序列号不同于window_base和next_send指针，为模MaxWindowSize的相对序列号</font>，这是GBN协议中最少使用的序列号，因为想要采用尽可能紧凑的协议设计。后续说明中，我们的数值比较，全部是在模MaxWindowSize意义下的。

​	我们按照如下逻辑对now_GBN进行维护：

1. **收到GBN序列号等于now_GBN的报文，即表明收到了期待的报文，接收端可以接受这个报文并将其写入，将now_GBN+1，并返回一个对应的ACK。**这里，我们还缓存了这个报文，类似于FSM中的sndokt，这实际上是为了<font color='red'>快速重传</font>，非必须。
2. **收到GBN序列号不等于now_GBN的报文，即表明收到了非期待的报文，我们丢弃这个报文，不做处理。**这里，我们实际上还返回了先前缓存的报文对应的ACK，这也是为了<font color='red'>快速重传</font>的实现。

​	基于上述处理，我们能保证接收端返回的ACK中，GBN序列号最大的报文为已经接收的报文，且其之前的报文也都已经接收，都可以进行发送端的确认，也就是保证了累计重传的正常实现。

#### （四）可靠的重传机制：超时重传和快速重传

​	重传机制的触发比较简单，我们先从什么时候重传开始回顾：

​	课程中实际上只重点讲了超时重传，但是实际上我还实现了快速重传，因为超时重传真的太慢了，二者的触发逻辑不同，但是重传的操作是一样的：

- **超时重传：**类似于发送端的FSM中，我们在每确认一个报文已接收后，都会开始计时，**当这个计时器超时，说明下一个报文迟迟无法确认接收，可能已经丢失了，需要重传**。
- **快速重传：**刚刚提到，我们在接收端接收到不对的报文时，还给客户端发送了先前已经确认接收报文的ACK，当客户端**连续接收到这种ACK时，认为报文已经丢失了**，需要重传。

​	如何进行重传操作呢？这又涉及到我们在FSM中见到的发送端sndpkt数组，我们同样也维护了这个数组，包含如下全局变量：

- **Data_message:**  该变量为一个大小为MaxWindowSize的报文数组，其中存储了窗口中已发送未确认的全部报文。
- **recv_GBN、send_GBN:**  这两个变量实际上是window_base和next_send指针在模MaxWindowSize意义下的映射，因为作为数组下标必须在范围内。

​	基于上述的缓冲区维护，我们在每次重传的时候，只需要从recv_GBN到send_GBN，在模MaxWindowSize意义下进行遍历Data_message数组，重传即可。

### **三、核心**功能代码分析

> **写在前面**：本次实验的代码在上一次实验的基础上给出，我们在报告中尽量只展示增加和删改的部分，避免过多冗余代码，造成报告过长。

​	在展开核心代码实现介绍前，我们先回顾一下先前提到的宏和全局变量，我们给出了如下的定义，此外还有一些多线程编程所需要的辅助全局变量。

##### 发送端：

```c++
// 滑动窗口所需的全局变量
// 接收到的GBN序列号和发送的GBN序列号
int recv_GBN = 0;
int send_GBN = 0;
// 滑动窗口基址和下一个要发送的序号
int window_base = 0;
int next_send = 0;
// 发送缓冲区
DATA_PACKAGE Data_message[MaxWindowSize];
```

​	这里注意的是，我们并没有像FSM中一样，将window_base和next_send设置为1，因为我们的传输过程中有一个开始传输信息，该信息尽管没有运用滑动窗口机制，但是还是需要增加一下window_base和next_send。

```c++
// 多线程所需的全局变量
int recv_mark = -1;  //返回标记
int wait[MaxWindowSize] = {0};  //停等标记
int	Fin_Trans = 1;	//传输结束标识
```

​	这里还有多线程的一些全局变量，返回标记用于阻塞模式下的计时，我们先前lab3-1已经提到，wait数组标记着当前Data_message中数据包报是否为发送未接收，辅助维护这个缓冲区，Fin-trans是消息接收线程的结束信号。

​	这里，我们实际上相当于只在发送端新建了一个消息接收线程，用于消息的接收处理。

##### 接收端：

​	接收端的全局变量非常简单，我们只维护了一个now_GBN，对应最近接收并写入的GBN序列号。

​	此外，因为我们的序列号是相对序列号，需要结合发送端窗口大小来维护，考虑到接收端可能不知道发送端窗口大小，我们把发送端窗口大小也发送过来了，放在0号数据包中，用MaxWindowSize这个全局变量暂存。

```c++
// 滑动窗口所需的全局变量
int now_GBN = 0; //最近接收并写入的GBN序列号
int MaxWindowSize;  //发送的滑动窗口大小
```

#### （一）多线程的引入

​	上一次实验中，我其实已经提前做了一部分多线程的处理，即我一开始想法是把发送改为多线程，但是在实际操作中发现会产生很多问题，而且不符合GBN的逻辑，我当时的理解并不到位。

​	重新复习所学的GBN协议后，我认识到，实际上我们的发送**没必要多线程进行，顺次进行即可**，而且当窗口太大的时候，发送线程会变得很多，可能产生问题，实际上发送并不会占用过多的资源（尽管我们是阻塞模式），完全可以顺次发送数据包，然后开一个接收线程，专门进行接收即可。

​	因此，在上一次的停等依次发送的代码中，我做了如下三点修改：

- 把**不必要的发送多线程，改为顺次发送**，调用函数sendMyMessage实现，其中完成消息的发送和日志输出。
- 同时把接收部分拿出来，在**发送前开启一个接收消息的线程**，即GetAckThread线程，在其中，我们完成消息的接收和处理。
- 修改循环继续的条件，原本是停等wait标记，上一个发送的接收到就继续，现在如果next_send - window_base >= MaxWindowSize，就表明滑动窗口没有空余，会阻塞循环，直至滑动窗口有空余，可以发送。

```c++
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
        
        ……中间是一些消息的处理和初始化
        
		SendMyMessage(Data_message[num]);
	}
	recv_mark = -1;
```

​	其中，SendMyMessage函数（主线程）和GetAckThread线程分别对应GBN协议状态机中的发送状态和接收状态，前者其实就是发送和日志输出，后者是核心的处理，涉及累计确认和滑动窗口实现，我们放到之后来具体讲解内容。

​	值得一提的是，为了保证多线程中日志输出能够正常输出，我增加了一个互斥锁，保证输出的有序性，如下为SendMyMessage函数的内容，其中日志输出就利用mtx互斥锁，保证不同播报的完整性和有序性。

```c++
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
```

#### （二）滑动窗口的实现

​	对于滑动窗口的实现，主要是对我们当时在实验原理部分提到的两个变量window_base和next_send进行维护来实现的，在先前的阻塞for循环的while代码中，我们能看到一个window_base++并break的代码，也就是这一部分：

​	对于next_send，我们先前的SendMyMessage中给出了其维护，即每次发送都给next _send加一。

```c++
while (next_send - window_base >= MaxWindowSize || get_wait() < 0) {
    if (get_wait()==1) {
        window_base++;
        break;
    }
}
```

​	这部分代码实现了对窗口基址的维护，也就是在窗口全满的时候进入循环卡死，如果出现空闲窗口，就增加window_base，并结束循环，继续发送。

​	此外，还需要注意发送结束后，此时窗口未超限，但是不会右移，也需要在发送后加上一个处理，同时在这里完成参数的重置，便于下一次发送：

```c++
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
```

​	以上对于window_base变量的维护都涉及到一个get_wait函数，这个函数是历史遗留，其实现比较粗糙，实际上就是看wait[send_GBN]的值是不是1，如果是1就说明窗口最左边的位已经空闲下来了，返回一个1，其中send_GBN是next_send的模MaxWindowSize映射。

​	而对wait数组的维护是在接收线程中实现的：

```c++
    while (recv_GBN != ACK_message.Udp_Header.GBN) {
        wait[recv_GBN] = 0;
        recv_GBN = (recv_GBN + 1) % MaxWindowSize;
    }
```

​	在接收到一个确认ACK后，会对recv_GBN和确认ACK区间内的所有wait置为0，并在模MaxWindowSize意义下更新recv_GBN。

​	基于上述处理，我们通过wait标记数组，间接性的来维护缓冲区内容和窗口内容，保证我们传输的准确性和正确性，实现了窗口的移动变化。

#### （三）累计确认实现

​	对于累计重传的实现，就需要详细来看看我们在server端做了哪些处理，和在client端的GetAckThread中做了哪些事情了。

​	首先我们这次实验其实对server端的修改不多，只是基于先前提到的now_GBN，每次收到新数据报都对其进行检查，看和now_GBN是否一致，实现顺序接收：

- 如果一致就正常返回ACK，将数据报写入并暂存，更新为最新的数据报。
- 如果不一致就丢弃，随后进行快速重传的ACK返回，这个ACK的GBN序列号是之前暂存的数据报。

​	我们将其接收数据报部分代码修改为如下代码：

```c++
for (int i = 0; i < package_num; i++) {
		DATA_PACKAGE Data_message;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
		while (1) {
			//不为负1的时候表明收到了消息
			if (recv_mark == 0) return false;
			//成功收到消息
			else if (recv_mark > 0) {
				//检查标记位和序列号ack，保证可靠性传输
				cout << "【接收数据播报】服务器端成功收到" << int(Data_message.Udp_Header.other) << "号数据包！" ;
				cout << "其中：GBN序列号为:" << int(Data_message.Udp_Header.GBN)  << ",期待的GBN序列号为" << now_GBN << endl;
				if ((Data_message.Udp_Header.GBN == now_GBN) && nameMessage.Udp_Header.other < Data_message.Udp_Header.other) {
					if (!check(Data_message)) return false;
					cout << "校验和为(对为1错为0):" << check(Data_message) << ",信息准确,接收" << int(Data_message.Udp_Header.other) << "号数据包！" << endl << endl;
					
					//lab2新增，维护GBN并用nameMessage暂存上一次接收的文件信息
					now_GBN = (now_GBN + 1) % MaxWindowSize;
					//实质上，这里的nameMessage就相当于我们的接收端缓冲区，为大小为1的窗口
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
					cout << "校验和为(对为1错为0):" << check(Data_message) << "，出现【非期待的数据包】！" << endl << endl;
					returnACK(nameMessage);
					//重新接收
					recv_mark = -1;
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
				}
				else {
					cout << "校验和为(对为1错为0):" << check(Data_message)<< "，收到小于累计范围的数据包，不做处理"<<endl << endl;
					//重新接收
					recv_mark = -1;
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvThread, &Data_message, 0, 0);
				}
			}
		}
		recv_mark = -1;
	}
```

​	然后，发送端还需要对接收端返回的ACK进行检查处理，如果一切正常，不需要启用重传机制，我们进行的检查如下：

- 即如果与上次确认的序列号的差（模MaxWindowSize意义）大于等于1，则表明这是一个新报文的ACK，我们可以采用先前提到的方式维护wait数组，进行滑动窗口的更新。
- 如果不然，则需要进行快速重传的检查，我们放到后面说。

```c++
DWORD WINAPI GetAckThread() {
	DATA_PACKAGE ACK_message;
	//这个实际上不是多线程，只是防止阻塞而已，因为是阻塞模式
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
				这一段里面是快速重传的处理
			}
			//重新接收
			recv_mark = -1;
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &ACK_message, 0, 0);
		}
		//超时，准备重传
		超时重传的处理
	}
	recv_mark = -1;
	return 0;
}
```

#### （四）重传机制实现

​	重传的实现非常简单，实际上就是对我们先前提到并维护的Data_message数组，按照窗口边界进行遍历重传即可，我们通过一个send_again函数实现：

```c++
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

```

​	而对于重传的检查，实际上都在接收线程中完成了，具体分为超时重传和快速重传。

​	超时重传就是我们先前在接收ack后都会重新开始计时，这里如果超时就会触发超时重传，具体代码如下：

```c++
    //超时，准备重传
    if (clock() - message_start > MaxWaitTime)
    {
        cout << ">>>>>>>>发送超时，准备重传全部信息......<<<<<<<<<" << endl<<endl;
        Send_Again(recv_GBN, MaxWindowSize);
        message_start = clock();
    }
```

​	快速重传，在我们这里实现是，如果连续收到三个一样已收到的确认报文，就需要进行重传，代码如下：

```c++
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
```

### 四、运行截图与传输结果分析

​	接下来，我们来看看正确运行的截图，和传输结果的分析：

#### （一）运行过程、结果及吞吐率、时间截图

##### 1、图片一

​	图片1，设置窗口大小为4，客户端和服务器端的传输过程日志和吞吐量、传输时间如下图所示，图片也已经上传到[github]([lll1230127/NKU2023-Network (github.com)](https://github.com/lll1230127/NKU2023-Network))：

![8](pic\8.jpg)

​	图片1的传输结果如下，发现能够正常显示，且属性一致：

![9](pic\9.jpg)

##### 2、图片二

​	图片2，设置窗口大小为8，客户端和服务器端的传输过程日志和吞吐量、传输时间如下图所示，图片也已经上传到[github]([lll1230127/NKU2023-Network (github.com)](https://github.com/lll1230127/NKU2023-Network))：

![10](pic\10.jpg)

​	图片2的传输结果如下，发现能够正常显示，且属性一致：

![11](pic\11.jpg)

##### 3、图片三

​	图片3，设置窗口大小为15，客户端和服务器端的传输过程日志和吞吐量、传输时间如下图所示，图片也已经上传到[github]([lll1230127/NKU2023-Network (github.com)](https://github.com/lll1230127/NKU2023-Network))：

![13](pic\13.jpg)

​	图片3的传输结果如下，发现能够正常显示，且属性一致：

![12](pic\12.png)

##### 4、文本

​	文本文件，设置窗口大小为32，客户端和服务器端的传输过程日志和吞吐量、传输时间如下图所示，图片也已经上传到[github]([lll1230127/NKU2023-Network (github.com)](https://github.com/lll1230127/NKU2023-Network))：

![15](pic\15.jpg)

​	文本的传输结果如下，发现能够正常显示，且属性一致：

![14](pic\14.jpg)

#### （二）传输结果汇总

|                  | 1.jpg  | 2.jpg  | 3.jpg  | 文本   |
| :--------------: | ------ | ------ | ------ | ------ |
|     窗口大小     | 4      | 8      | 15     | 32     |
|   传输时间(s)    | 5      | 34     | 81     | 11     |
| 吞吐率（byte/s） | 371471 | 173485 | 147765 | 150528 |



### 五、实验总结和检查问题分析

#### （一）检查问题回顾

​	本次检查中，对于助教老师提问的问题，回答的都还行。但是其中对于累计确认的理解，我按照书本给出了正确回答，但心中有点疑惑，即书中对于累计确认的概念，侧重于发送端，忽视了接受端的贡献。

​	助教老师为我解释了这个疑惑的原因，实际上累计确认是类似于一种协议的概念，而这个概念的核心就体现在发送端的处理上，使得我对累计确认有了新的认识。

#### （二）实验总结

​	本次实验我们实现了基于GBN协议的可靠传输，能够实现有滑动窗口的累计确认。这里，我为了提高效率，增加了快速重传的处理。实质上，可能还有别的处理方法，等待我们去探索，不过这不是我们实验的核心，就没有过多研究。

​	比如，对于累计确认，我们可以在接收端不收到一个报文就发送一个ACK，而是接收一部分之后返回一个正确的最大的，这样就可以节约资源。

​	此外，实验中的一些bug也加深了我的理解。对于缓冲区的维护，要求send中的缓冲区不能是一个临时变量，此外，c++的模操作结果的正负是取决于前一个操作数，这个有点忘了，导致自己修了很久bug。

​	最后，我还思考了一下GBN的缺点，从先前图片分析结果我们也能看到，当窗口较大、且丢包率较高的时候，传输效率很差，这是因为重传的代价变得很大。

​	这就是为什么我们需要SR的实现，当然，这是下一次实验报告需要探讨的问题了。

​	
