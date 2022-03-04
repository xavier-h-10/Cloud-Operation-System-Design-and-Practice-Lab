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

message *message_buffer[WINDOW_SIZE];
int message_num[WINDOW_SIZE];

int ack;

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

//    for (int i = 0; i < WINDOW_SIZE; i++) {
//        message_buffer[i] =(struct message *) malloc(sizeof(struct message));
//    }
//    buffer_valid = new bool[WINDOW_SIZE];

//    for (int i = 0; i < WINDOW_SIZE; i++) {
//        buffer_valid[i] = false;
//    }

    for (int i = 0; i < WINDOW_SIZE; i++) {
        message_num[i] = -1;
    }
    ack = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
//    for (int i = 0; i < WINDOW_SIZE; i++) {
//        if(message_buffer[i]!=NULL) {
//            if(message_buffer[i]->data!=NULL) free(message_buffer[i]->data);
//            free(message_buffer[i]);
//        }
//    }
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
    std::cout << "send ack , ack=" << ack << std::endl;
    packet *pkt = new packet;         // ack包只包含checksum和ack即可
    memcpy(pkt->data + 2, &ack, sizeof(int));

    unsigned short checksum = calc_Checksum(pkt);
    memcpy(pkt->data, &checksum, sizeof(short));
    std::cout<<" send ack check 1"<<std::endl;
    Receiver_ToLowerLayer(pkt);
}


/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
    int header_size = 7;  //checksum + payload_size + seq

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message *) malloc(sizeof(struct message));

    //检查checksum,若checksum不正确则丢包
    unsigned short checksum;
    memcpy(&checksum, pkt->data, sizeof(short));
    memcpy(&msg->size, pkt->data + 2, 1);
    if (checksum != calc_Checksum(pkt) || msg->size < 0 || msg->size > RDT_PKTSIZE - header_size) {
        std::cout << "packet corrupted" << std::endl;
        return;
    }

    int seq;
    memcpy(&seq, pkt->data + 3, sizeof(int));

    std::cout << "receive packet, seq=" << seq << " ack=" << ack << " size=" << msg->size << std::endl;
    if (seq >= ack + WINDOW_SIZE) return; //超出当前滑动窗口，因此不接收

    if (seq < ack) {           //此时说明ack发生了丢包，需要重传ack包
        send_ack(ack - 1);
    }

    msg->data = (char *) malloc(msg->size);
    std::cout << "checkpoint 1" << std::endl;
    memcpy(msg->data, pkt->data + header_size, msg->size);
    std::cout << "seq=" << seq << " " << msg->data << std::endl;
    message_buffer[seq % WINDOW_SIZE] = msg;
    message_num[seq % WINDOW_SIZE] = seq;

    std::cout << "checkpoint 2 " << seq << " " << ack << std::endl;
    if (seq == ack) {
        std::cout << "checkpoint 2.0.1" << std::endl;
        Receiver_ToUpperLayer(msg);
        std::cout << "checkpoint 2.0.2" << std::endl;
        while (true) {
            ack++;
            std::cout<<"checkpoint 2.1.0 "<<ack<<std::endl;
            message *tmp = message_buffer[ack % WINDOW_SIZE];
            seq = message_num[ack % WINDOW_SIZE];
            if (tmp == nullptr || tmp->size <= 0 || tmp->size > RDT_PKTSIZE || seq != ack) break;
            std::cout << "checkpoint 2.1 " << seq << " " << ack << " " << tmp->size << " " << strlen(tmp->data)
                      << std::endl;
//            if (seq == ack) {
//                Receiver_ToUpperLayer(tmp);
//            } else {
//                break;
//            }
            Receiver_ToUpperLayer(tmp);
        }
        send_ack(ack - 1);
    }

    std::cout << "checkpoint 3" << std::endl;


//    msg->data = (char *) malloc(msg->size);
//    ASSERT(msg->data != NULL);
//    memcpy(msg->data, pkt->data + header_size, msg->size);
//    Receiver_ToUpperLayer(msg);

    /* don't forget to free the space */
//    if (msg->data != NULL) free(msg->data);
//    if (msg != NULL) free(msg);
}
