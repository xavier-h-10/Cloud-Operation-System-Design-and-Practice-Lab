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
#define BUFFER_SIZE 20000


message *buffer;
packet *window;

int num_message;
int mx_ack;
int current_num;
int next_num;

int cursor;

int now_size;
int mx_seq;
int next_seq;
const int header_size = 7;


/* sender initialization, called once at the very beginning */
void Sender_Init() {
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    buffer = (message *) malloc((BUFFER_SIZE) * sizeof(message));
    window = (packet *) malloc(WINDOW_SIZE * sizeof(packet));
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

void convert() {
    int idx = current_num % BUFFER_SIZE;
    message msg = buffer[idx];
    packet pkt;

    while (now_size < WINDOW_SIZE && current_num < next_num) {
        if (msg.size - cursor > RDT_PKTSIZE - header_size) {
            memcpy(pkt.data + 2, &mx_seq, 4);
            pkt.data[header_size - 1] = RDT_PKTSIZE - header_size;  //TODO: 用memcpy有问题
            memcpy(pkt.data + header_size, msg.data + cursor, RDT_PKTSIZE - header_size);

            short checksum = calc_checksum(&pkt);
            memcpy(pkt.data, &checksum, 2);
            memcpy(window[mx_seq % WINDOW_SIZE].data, &pkt, sizeof(packet));
            mx_seq++;
            now_size++;
            cursor += RDT_PKTSIZE - header_size;
        } else if (cursor < msg.size) {
            memcpy(pkt.data + 2, &mx_seq, 4);
            pkt.data[header_size - 1] = msg.size - cursor;
            memcpy(pkt.data + header_size, msg.data + cursor, msg.size - cursor);

            short checksum = calc_checksum(&pkt);
            memcpy(pkt.data, &checksum, 2);
            memcpy(window[mx_seq % WINDOW_SIZE].data, &pkt, sizeof(packet));
            current_num++;
            cursor = 0;
            num_message--;
            mx_seq++;
            now_size++;
            if (current_num < next_num) msg = buffer[current_num % BUFFER_SIZE];
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
    int idx = next_num % BUFFER_SIZE;
    buffer[idx].size = msg->size + 4;
    buffer[idx].data = (char *) malloc(msg->size + 4);
    memcpy(buffer[idx].data, &msg->size, 4);
    memcpy(buffer[idx].data + 4, msg->data, msg->size);
    next_num++;
    num_message++;
    if (Sender_isTimerSet()) return;

    Sender_StartTimer(TIMEOUT);
    convert();
    send_packet();
}


/* event handler, called when the timer expires */
void Sender_Timeout() {
    next_seq = mx_ack;  //重新发mx_ack包
    Sender_StartTimer(TIMEOUT);
    convert();
    send_packet();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {
    short checksum;
    memcpy(&checksum, pkt->data, 2);
    if (checksum != calc_checksum(pkt)) {
        return;
    }

    int ack_seq;
    memcpy(&ack_seq, pkt->data + 2, 4);
    if (mx_ack <= ack_seq && ack_seq < mx_seq) {  //删掉全部window里的包
        //       std::cout<<"sender checkpoint:"<<mx_ack<<" "<<ack_seq<<" "<<mx_seq<<std::endl;
        now_size = now_size + mx_ack - ack_seq - 1;
        mx_ack = ack_seq + 1;
        Sender_StartTimer(TIMEOUT);
        convert();
        send_packet();
    }
    if (ack_seq == mx_seq - 1) {   //当前所有包都被接收，停止计时器
        Sender_StopTimer();
    }
}

