/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: In my implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  2 bytes  ->|<-  4 bytes ->|<-             the rest            ->|
 *       | checksum     |  seq number  |<-             payload             ->|
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

message **message_buffer;
int ack;

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

    message_buffer = new message *[WINDOW_SIZE];
//    buffer_valid = new bool[WINDOW_SIZE];

//    for (int i = 0; i < WINDOW_SIZE; i++) {
//        buffer_valid[i] = false;
//    }
    ack = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
    delete message_buffer;
//    delete buffer_num;
}

unsigned short calc_Checksum(struct packet *pkt) {
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
    std::cout<<"send ack , ack="<<ack<<std::endl;
    packet pkt;         // ack包只包含checksum和ack即可
    memcpy(pkt.data + 2, &ack, sizeof(int));

    unsigned short checksum = calc_Checksum(&pkt);
    memcpy(pkt.data, &checksum, sizeof(short));

    Receiver_ToLowerLayer(&pkt);
}


/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
    int header_size = 7;  //checksum + payload_size + seq

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message *) malloc(sizeof(struct message));
    ASSERT(msg != NULL)

    //检查checksum,若checksum不正确则丢包
    unsigned short checksum;
    memcpy(&checksum, pkt->data, sizeof(short));
    if (checksum != calc_Checksum(pkt) || msg->size < 0 || msg->size > RDT_PKTSIZE - header_size) {
        return;
    }

    memcpy(&msg->size, pkt->data + 2, 1);
    int seq;
    memcpy(&seq, pkt->data + 3, sizeof(int));

    std::cout<<"receive packet, seq="<<seq<<" ack="<<ack<<std::endl;
    if (seq >= ack + WINDOW_SIZE || seq < ack) return; //超出当前滑动窗口，因此不接收
    msg->data = (char *) malloc(msg->size);
    ASSERT(msg->data != NULL);
    memcpy(msg->data, pkt->data + header_size, msg->size);
    message_buffer[seq % WINDOW_SIZE] = msg;

    if (seq == ack) {
        Receiver_ToUpperLayer(msg);
        while (true) {
            ack++;
            message *tmp = message_buffer[ack % WINDOW_SIZE];
            if(tmp==nullptr) break;
            memcpy(&seq, tmp->data + 3, sizeof(int));
            if (seq == ack) {
                Receiver_ToUpperLayer(tmp);
            } else {
                break;
            }
        }
        send_ack(ack - 1);
    }


//    msg->data = (char *) malloc(msg->size);
//    ASSERT(msg->data != NULL);
//    memcpy(msg->data, pkt->data + header_size, msg->size);
//    Receiver_ToUpperLayer(msg);

    /* don't forget to free the space */
    if (msg->data != NULL) free(msg->data);
    if (msg != NULL) free(msg);
}
