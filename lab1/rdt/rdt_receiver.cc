/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: In my implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  2 bytes  ->|<-  4 bytes ->|<-             the rest            ->|
 *       |   checksum    |  ack number  |              payload                |
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "rdt_struct.h"
#include "rdt_receiver.h"

#define WINDOW_SIZE 10

static packet *packet_buffer;

bool valid[WINDOW_SIZE];

static message *msg;
static int cursor;

static int ack;

const int header_size = 7; //checksum + size + seq

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

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

    msg = (message *) malloc(sizeof(message));
    packet_buffer = (packet *) malloc(WINDOW_SIZE * sizeof(packet));
    memset(valid,0,sizeof(valid));
    ack = 0;
    cursor = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

void send_ack(int ack) {
    packet pkt;          // ack包只包含checksum和ack即可
    memcpy(pkt.data + 2, &ack, 4);
    short checksum = calc_checksum(&pkt);
    memcpy(pkt.data, &checksum, 2);
    Receiver_ToLowerLayer(&pkt);
}


/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
    //检查checksum,若checksum不正确则丢包
    short checksum;
    memcpy(&checksum, pkt->data, 2);
    if (checksum != calc_checksum(pkt)) { // 校验失败
        return;
    }

    int seq;
    memcpy(&seq, pkt->data + 2, 4);
    if (ack < seq && seq < ack + WINDOW_SIZE) {   //滑动窗口范围内
        int idx = seq % WINDOW_SIZE;
        if (!valid[idx]) {
            memcpy(packet_buffer[idx].data, pkt->data, RDT_PKTSIZE);
            valid[idx] = true;
        }
        send_ack(ack - 1);
        return;
    } else if (seq != ack) {
        send_ack(ack - 1);
    }

    if (seq == ack) { //seq=ack, 考虑buffer中的包
        int size;
        while (true) {
            ack++;
            size = (int) pkt->data[header_size - 1];

            if (cursor == 0) {  //第一个包有message size, +4
                size -= 4;
//                std::cout<<"checkpoint 1";
                memcpy(&msg->size, pkt->data + header_size, 4);
                msg->data = (char *) malloc(msg->size);
//                std::cout<<"checkpoint 2";
                memcpy(msg->data + cursor, pkt->data + header_size + 4, size);
                cursor += size;
            } else {
                memcpy(msg->data + cursor, pkt->data + header_size, size);
                cursor += size;
            }

            if (cursor == msg->size) {      //整个message组装好发送
                Receiver_ToUpperLayer(msg);
                cursor = 0;
            }

            int idx = ack % WINDOW_SIZE;
            if (valid[idx] == 1) {
//                std::cout<<"valid=1, idx="<<idx<<std::endl;
                pkt = &packet_buffer[idx];
                memcpy(&seq, pkt->data + 2, 4);
                valid[idx] = 0;
            } else {
                send_ack(seq);
                break;
            }
        }
    }
}