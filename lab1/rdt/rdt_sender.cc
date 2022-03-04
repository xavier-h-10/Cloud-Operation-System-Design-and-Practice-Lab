/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE:  In my implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  2 bytes  ->|<-  4 bytes ->|<-  1 byte ->|<-             the rest            ->|
 *       |   checksum   |  seq number  | payload size |            payload                 |
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <queue>

#include "rdt_struct.h"
#include "rdt_sender.h"

#define TIMEOUT 0.3
#define WINDOW_SIZE 10
#define MAX_BUFFER_SIZE 15000

// 消息缓存
static message* message_buffer;

// message_buffer中消息的数量
static int num_message;

// 当前正在处理的消息序号
static int current_num;

// 下一个进入message_buffer的消息的序号
static int next_num;

// 当前message已被拆分的byte数量
static int cursor;

// 存储待发送的数据包，大小为WINDOW_SIZE
static packet* window;


// window中的包的数量
static int now_size; // now_size维护目前buffer的大小

// 下一个进入window的数据包的序号
static int mx_seq;

// 下一个要被发送的数据包序号
static int next_seq;

// 应该接收到的ack序号
static int mx_ack;
const int header_size=7;


/* sender initialization, called once at the very beginning */
void Sender_Init() {
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    message_buffer = (message *)malloc((MAX_BUFFER_SIZE) * sizeof(message));
    memset(message_buffer, 0, (MAX_BUFFER_SIZE) * sizeof(message));
    num_message = 0;
    current_num = 0;
    next_num = 0;
    cursor = 0;

    window = (packet *)malloc(WINDOW_SIZE * sizeof(packet));
    memset(window, 0, WINDOW_SIZE * sizeof(packet));
    now_size = 0;
    mx_seq = 0;
    next_seq = 0;
    mx_ack = 0;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final() {
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

static short calc_checksum(struct packet *pkt) {
    long sum = 0;
    for (int i = 2; i < RDT_PKTSIZE; i += 2) {
        sum += *(unsigned short *) (&(pkt->data[i]));
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

void Chunk_Message() {
    int idx = current_num % MAX_BUFFER_SIZE;
    message msg = message_buffer[idx];
    packet pkt;

    while (now_size < WINDOW_SIZE && current_num < next_num) {
        if (msg.size - cursor > RDT_PKTSIZE - header_size) {
            memcpy(pkt.data + 2, &mx_seq, 4);
            pkt.data[header_size - 1] = RDT_PKTSIZE - header_size;  //TODO: 用memcpy有问题
            memcpy(pkt.data + header_size, msg.data + cursor, RDT_PKTSIZE - header_size);
            // 计算checksum
            short checksum = calc_checksum(&pkt);
            memcpy(pkt.data, &checksum, 2);
            // 将pkt的内容存入window，位置为next_packet_index
            int next_packet_index = mx_seq % WINDOW_SIZE;
            memcpy(window[next_packet_index].data, &pkt, sizeof(packet));
            cursor += RDT_PKTSIZE - header_size;
            ++mx_seq;
            ++now_size;
        }
            // 剩余待拆分的byte可以一次性装入一个packet
        else if (cursor < msg.size) {
            // 写包头和数据
            memcpy(pkt.data + 2, &mx_seq, 4);
            pkt.data[header_size - 1] = msg.size - cursor;
            memcpy(pkt.data + header_size, msg.data + cursor, msg.size - cursor);
            short checksum = calc_checksum(&pkt);
            memcpy(pkt.data, &checksum, 2);
            memcpy(window[mx_seq % WINDOW_SIZE].data, &pkt, sizeof(packet));
            ++mx_seq;
            ++now_size;
            // 当前消息拆分完毕，将其从message_buffer中移除
            ++current_num;
            cursor = 0;
            --num_message;
            // 判断message_buffer中是否还有未拆分的message
            assert(current_num + num_message == next_num);
            if (current_num < next_num) {
                int idx = current_num % MAX_BUFFER_SIZE;
                msg = message_buffer[idx];
            }
        }
    }
}

void send_packet() {
    /* send it out through the lower layer */
    packet pkt;
    while (next_seq < mx_seq) {
        memcpy(&pkt, &(window[next_seq % WINDOW_SIZE]), sizeof(packet));
        Sender_ToLowerLayer(&pkt);
        ++next_seq;
    }
    //    Sender_StartTimer(TIMEOUT);
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg) {

    int idx = next_num % MAX_BUFFER_SIZE;
    message_buffer[idx].size = msg->size + 4;
    message_buffer[idx].data = (char *)malloc(message_buffer[idx].size);
    memcpy(message_buffer[idx].data, &(msg->size), 4); // 先将msg->size保存到data中
    memcpy(message_buffer[idx].data + 4, msg->data, msg->size);
    ++next_num;
    ++num_message;

    // 若计时器仍在计时，则不用操作，新到的message老老实实待在buffer里
    if (Sender_isTimerSet() == true) {
        return ;
    }

    // 计时器没有工作，则开始新一轮发包
    Sender_StartTimer(TIMEOUT);
    Chunk_Message();
    send_packet();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {
    // 检查checksum，校验失败则直接抛弃
    short checksum;
    memcpy(&checksum, pkt->data, 2);
    if (checksum != calc_checksum(pkt)) { // 校验失败
        return ;
    }

    int ack_seq;
    memcpy(&ack_seq, pkt->data + 2, 4);
    // 若收到的ack_seq在正常范围，则说明小于ack的packet都已经被receiver正常接收
    if (mx_ack <= ack_seq && ack_seq < mx_seq) {
        now_size -= (ack_seq - mx_ack + 1); // 确认接收的包均从window中可移除
        mx_ack = ack_seq + 1;
        // 开始新一轮发包
        Sender_StartTimer(TIMEOUT); // 若上一轮计时没有结束，则该命令重置计时器
        Chunk_Message();
        send_packet();
    }
    // 若window中所有的包都已确认被receiver接收，则结束计时器
    if (ack_seq == mx_seq - 1) {
        Sender_StopTimer();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout() {
    // 超时，该收到的ack包没收到，重发
    next_seq = mx_ack;
    Sender_StartTimer(TIMEOUT);
    Chunk_Message();
    send_packet();
}
