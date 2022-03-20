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

message *message_buffer[WINDOW_SIZE];
int message_seq[WINDOW_SIZE];   //对应的buffer seq number

int ack;

int total_size;

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

    for (int i = 0; i < WINDOW_SIZE; i++) {
        message_seq[i] = -1;
    }
    ack = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

unsigned short calc_checksum(struct packet *pkt) {
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
    packet *pkt = new packet;         // ack包只包含checksum和ack即可
    memcpy(pkt->data + 2, &ack, 4);

    unsigned short checksum = calc_checksum(pkt);
    memcpy(pkt->data, &checksum, 2);
    Receiver_ToLowerLayer(pkt);
}


/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
    int header_size = 7;  //checksum + payload_size + seq

    /* construct a message and deliver to the upper layer */
    message *msg = new message;
    memcpy(&msg->size, pkt->data + 2, 1);

    //检查checksum,若checksum不正确则丢包
    unsigned short checksum;
    memcpy(&checksum, pkt->data, 2);
    if (checksum != calc_checksum(pkt) || msg->size <= 0 || msg->size > RDT_PKTSIZE - header_size) {
        return;
    }

    int seq;
    memcpy(&seq, pkt->data + 3, 4);

    if (seq >= ack + WINDOW_SIZE) return; //超出当前滑动窗口，因此不接收

    if (seq < ack) {           //此时说明ack包发生了丢包，需要重传ack包
        send_ack(ack - 1);
    }

    msg->data = new char[msg->size];
    memcpy(msg->data, pkt->data + header_size, msg->size);

    message_buffer[seq % WINDOW_SIZE] = msg;
    message_seq[seq % WINDOW_SIZE] = seq;

    if (seq == ack) {
        Receiver_ToUpperLayer(msg);
        total_size += strlen(msg->data);
        std::cout << "receiver size=" << total_size << std::endl;

        while (true) {
            ack++;
            message *tmp = message_buffer[ack % WINDOW_SIZE];
            seq = message_seq[ack % WINDOW_SIZE];
            if (tmp == nullptr || seq != ack) break;
            total_size += strlen(tmp->data);
            std::cout << "receiver size=" << total_size << std::endl;
            Receiver_ToUpperLayer(tmp);
        }
        send_ack(ack - 1);
    }
}
