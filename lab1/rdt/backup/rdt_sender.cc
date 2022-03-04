/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE:  In my implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  2 bytes  ->|<-  1 byte ->|<-  4 bytes ->|<-             the rest            ->|
 *       |   checksum   | payload size |  seq number  |            payload                 |
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <queue>
#include <iostream>

#include "rdt_struct.h"
#include "rdt_sender.h"

#define TIMEOUT 0.3
#define WINDOW_SIZE 10

int mx_seq, mx_ack, now_size;   // now_size维护目前buffer的大小

int tot_size=0;

packet *window[WINDOW_SIZE];
std::queue<packet *> buffer;

/* sender initialization, called once at the very beginning */
void Sender_Init() {
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    mx_seq = 0;
    mx_ack = -1;
    now_size = 0;
    while (!buffer.empty()) buffer.pop();
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final() {
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
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

void send_packet(packet *pkt) {
    /* send it out through the lower layer */
    if (now_size < WINDOW_SIZE) {
        window[now_size++] = pkt;
        Sender_ToLowerLayer(pkt);
    } else {
        buffer.push(pkt);
    }
//    Sender_StartTimer(TIMEOUT);
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(message *msg) {
    int header_size = 7;

    tot_size+=msg->size;
    std::cout<<"sender package, size="<<tot_size<<std::endl;

    /* maximum payload size */
    char maxpayload_size = (char) (RDT_PKTSIZE - header_size);

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size - cursor > maxpayload_size) {
        /* fill in the packet */
        packet *pkt = new packet;
        memcpy(pkt->data + 2, &maxpayload_size, 1);
        memcpy(pkt->data + 3, &mx_seq, 4);
        memcpy(pkt->data + header_size, msg->data + cursor, maxpayload_size);
        unsigned short checksum = calc_Checksum(pkt);
        memcpy(pkt->data, &checksum, 2);
        send_packet(pkt);
        mx_seq++;

        /* move the cursor */
        cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
        /* fill in the packet */
        packet *pkt = new packet;
        char tmp = char(msg->size - cursor);
        memcpy(pkt->data + 2, &tmp, 1);
        memcpy(pkt->data + 3, &mx_seq, 4);
        memcpy(pkt->data + header_size, msg->data + cursor, (size_t) tmp);
        unsigned short checksum = calc_Checksum(pkt);
        memcpy(pkt->data, &checksum, 2);
        send_packet(pkt);
        mx_seq++;
    }
    Sender_StartTimer(TIMEOUT);
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {
    //检查checksum,若checksum不正确则丢包
    unsigned short checksum;
    memcpy(&checksum, pkt->data, 2);
    if (checksum != calc_Checksum(pkt)) {
        return;
    }
    int ack;
    memcpy(&ack, pkt->data + 2, 4);

    if (ack <= 0) return;

    std::cout << "receive ack, ack=" << ack << std::endl;
    if (ack > mx_ack) {
        mx_ack = ack;
        if (now_size == WINDOW_SIZE) {
            int seq;

            packet *tmp = window[now_size - 1];
            memcpy(&seq, tmp->data + 3, sizeof(int));
            std::cout << "window is full, seq=" << seq << " ack=" << ack <<" buffer_size="<<buffer.size()<< std::endl;
            if (seq <= ack) {
                std::cout << " clear window, send packet" << std::endl;
                for (int i = 0; i < WINDOW_SIZE; i++) {
                    delete window[i];
                }
                now_size = 0;
                int size = std::min((int) buffer.size(), WINDOW_SIZE);
                for (int i = 0; i < size; i++) {
                    window[now_size++] = buffer.front();
                    Sender_ToLowerLayer(window[i]);
                    buffer.pop();
                }
                //  Sender_StartTimer(TIMEOUT);
            }
        }
    }
    Sender_StartTimer(TIMEOUT);
}

/* event handler, called when the timer expires */
void Sender_Timeout() {
    bool start_timer = false;
    for (int i = 0; i < now_size; i++) {
        int seq;
        memcpy(&seq, window[i]->data + 3, sizeof(int));
        if (seq > mx_ack) {
//            std::cout << "packet resent seq=" << seq << " ack=" << mx_ack << std::endl;
            Sender_ToLowerLayer(window[i]);
            start_timer = true;
        }
    }
    if (start_timer) {
        Sender_StartTimer(TIMEOUT);
    } else {
        if (buffer.empty()) return;
//        for (int i = 0; i < WINDOW_SIZE; i++) {
//            delete window[i];
//        }
        now_size = 0;
        int size = std::min((int) buffer.size(), WINDOW_SIZE);
        for (int i = 0; i < size; i++) {
            window[now_size++] = buffer.front();
            Sender_ToLowerLayer(window[i]);
            buffer.pop();
        }
        Sender_StartTimer(TIMEOUT);
    }
}
