/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or
 *       reordering.  You will need to enhance it to deal with all these
 *       situations.  In this implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
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

#define DEBUG 1

int mx_seq, mx_ack, now_size;
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


void send_packet(struct packet *pkt) {
    /* send it out through the lower layer */
    if (now_size < WINDOW_SIZE) {
//        window.push_back(pkt);
        window[now_size++] = pkt;
        Sender_ToLowerLayer(pkt);

        //   memcpy(&tmp, pkt->data + 3, sizeof(int));
        //       std::cout << "packet sent, add to window, seq=" << tmp << " size=" << window.size() << " " << pkt << std::endl;
//        memcpy(&tmp, window[window.size() - 1]->data + 3, sizeof(int));

//        std::cout << tmp << std::endl;
    } else {
        buffer.push(pkt);
    }
//    Sender_StartTimer(TIMEOUT);
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg) {
    /* 1-byte header indicating the size of the payload */
    int header_size = 7;

    /* maximum payload size */
    char maxpayload_size = (char) (RDT_PKTSIZE - header_size);

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size - cursor > maxpayload_size) {
        /* fill in the packet */
//        pkt->data[2] = maxpayload_size;  //payload size
        packet *pkt = new packet;
        memcpy(pkt->data + 2, &maxpayload_size, 1);
        memcpy(pkt->data + 3, &mx_seq, sizeof(int));
        memcpy(pkt->data + header_size, msg->data + cursor, maxpayload_size);
        unsigned short checksum = calc_checksum(pkt);
        memcpy(pkt->data, &checksum, sizeof(short));

//        char pp[128];
//        memcpy(pp, pkt->data + header_size, maxpayload_size);
//        std::cout << "send packet, seq=" << mx_seq << " " << pp << std::endl;
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
        memcpy(pkt->data + 3, &mx_seq, sizeof(int));
        memcpy(pkt->data + header_size, msg->data + cursor, tmp);
        unsigned short checksum = calc_checksum(pkt);
        memcpy(pkt->data, &checksum, sizeof(short));

//        char pp[128];
//        memcpy(pp, pkt->data + header_size, tmp);
//
//        std::cout << "send last packet, seq=" << mx_seq << " " << pp << std::endl;
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
    memcpy(&checksum, pkt->data, sizeof(short));
    if (checksum != calc_checksum(pkt)) {
        return;
    }
    int ack;
    memcpy(&ack, pkt->data + 2, sizeof(int));

    std::cout << "receive ack, ack=" << ack << std::endl;
    if (ack > mx_ack) {
        mx_ack = ack;
        if (now_size == WINDOW_SIZE) {
            int seq;

            packet *tmp = window[now_size - 1];
            memcpy(&seq, tmp->data + 3, sizeof(int));
            std::cout << "see seq=" << seq << std::endl;
            std::cout << "window is full, seq=" << seq << " ack=" << ack <<" buffer_size="<<buffer.size()<< std::endl;
            if (seq <= ack) {
                std::cout << " clear window, send packet" << std::endl;
//                for (int i = 0; i < WINDOW_SIZE; i++) {
//                    delete window[i];
//                }
//                window.clear();
                now_size = 0;
                int size = std::min((int) buffer.size(), WINDOW_SIZE);
                for (int i = 0; i < size; i++) {
                    std::cout << "i=" << i << std::endl;
                    window[now_size++] = buffer.front();
                    std::cout << "check 3.1" << std::endl;
                    Sender_ToLowerLayer(window[i]);
                    std::cout << "check 3.2" << std::endl;
                    buffer.pop();
                    std::cout << "check 3.3" << std::endl;
                }
                Sender_StartTimer(TIMEOUT);
            }
        }
    }

//    if(pkt) delete pkt;  // 收到ack包后,回收空间
}

/* event handler, called when the timer expires */
void Sender_Timeout() {
    bool start_timer = false;
    std::cout << "timeout, window.size=" << now_size << " buffer.size=" << buffer.size() << std::endl;
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
//            window.push_back(buffer.front());
            window[now_size++] = buffer.front();
            Sender_ToLowerLayer(window[i]);
            buffer.pop();
        }
        Sender_StartTimer(TIMEOUT);
    }
}
