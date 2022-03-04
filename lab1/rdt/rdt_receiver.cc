/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 *              In this implementation, the packet format is laid out as the following:
 *
 *     1. The first packet of a message from sender to receiver
 *       |<-  2 byte  ->|<-  4 byte  ->|<-  1 byte  ->|<-  4 byte  ->|<-  the rest  ->|
 *       |   checksum   |  packet seq  | payload size | message size |     payload    |
 *
 *     2. The rest packets of a message from sender to receiver
 *       |<-  2 byte  ->|<-  4 byte  ->|<-  1 byte  ->|<-  the rest  ->|
 *       |   checksum   |  packet seq  | payload size |     payload    |
 *
 *     3. The ACK packet from receiver to sender
 *       |<-  2 byte  ->|<-  4 byte  ->|<-  the rest  ->|
 *       |   checksum   |    ack seq   |   meaningless  |
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "rdt_struct.h"
#include "rdt_receiver.h"

#define WINDOW_SIZE 10

// 当前正在接受的message
static message *current_message;

// 当前message已经构建完的byte数量
static int message_cursor;

// 接收方的数据包缓存
static packet *receiver_buffer;

// 数据包缓存的有效位
static char *valid;

// 应该收到的packet seq
static int ack;

const int header_size = 7; //checksum + payload_size + seq

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

void send_ack(int ack) {
    packet pkt;          // ack包只包含checksum和ack即可
    memcpy(pkt.data + 2, &ack, 4);
    short checksum = calc_checksum(&pkt);
    memcpy(pkt.data, &checksum, 2);
    Receiver_ToLowerLayer(&pkt);
}


/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

    // 初始化current_message
    current_message = (message *) malloc(sizeof(message));

    // 初始化receiver_buffer和valid
    receiver_buffer = (packet *) malloc(WINDOW_SIZE * sizeof(packet));
    valid = (char *) malloc(WINDOW_SIZE);

    // 其他初始化
    ack = 0;
    message_cursor = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
    // 检查checksum，校验失败则直接抛弃
    short checksum;
    memcpy(&checksum, pkt->data, 2);
    if (checksum != calc_checksum(pkt)) { // 校验失败
        return;
    }

    int seq;
    memcpy(&seq, pkt->data + 2, 4);
    if (ack < seq &&
        seq < ack + WINDOW_SIZE) {
        int idx = seq % WINDOW_SIZE;
        if (valid[idx] == 0) {
            memcpy(receiver_buffer[idx].data, pkt->data, RDT_PKTSIZE);
            valid[idx] = 1;
        }
        send_ack(ack - 1);
        return;
    } else if (seq != ack) {
        send_ack(ack - 1);
        return;
    } else if (seq == ack) { // 收到了想要的数据包
        int payload_size;
        while (1) {
            ++ack;
            // memcpy(&payload_size, pkt->data + 2 + 4, sizeof(char));
            // WAERNING: 如果用上面的方法给payload_size赋值会出错！
            payload_size = pkt->data[header_size - 1];

            // 判断是不是第一个包，并将payload写入current_message
            if (message_cursor == 0) {
                if (current_message->size != 0) {
                    current_message->size = 0;
                    free(current_message->data);
                }
                payload_size -= 4; // 减去message_size占用的4个byte
                memcpy(&(current_message->size), pkt->data + header_size, 4);
                current_message->data = (char *) malloc(current_message->size);
                memcpy(current_message->data + message_cursor, pkt->data + header_size + 4, payload_size);
                message_cursor += payload_size;
            } else {
                memcpy(current_message->data + message_cursor, pkt->data + header_size, payload_size);
                message_cursor += payload_size;
            }

            // 检查current_message是否构建完成
            if (message_cursor == current_message->size) {
                Receiver_ToUpperLayer(current_message);
                message_cursor = 0;
            }

            int idx = ack % WINDOW_SIZE;
            if (valid[idx] == 1) {
                pkt = &receiver_buffer[idx];
                memcpy(&seq, pkt->data + 2, 4);
                valid[idx] = 0;
            } else {
                send_ack(seq);
                return;
            }
        }
    }
}