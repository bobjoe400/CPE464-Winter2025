#include <stdio.h>
#include <pcap.h>
#include "checksum.h"
#include "utility.h"
#include <sys/types.h>
#include <string.h>

//just pass in the current packet pointer and check the type
void process_icmp_hdr(const unsigned char* packet){
    printf("\tICMP Header\n");

    printf("\t\tType: ");
    char type = packet++[0];
    if(type == 8){
        printf("Request\n");
    }else if(type == 0){
        printf("Reply\n");
    }else{
        printf("%i\n", type);
    }
}

//just pass in the current packet pointer and get the two ports
void process_udp_hdr(const unsigned char* packet){
    printf("\tUDP Header\n");

    printf("\t\tSource Port:  ");
    print_tcp_udp_port(&packet);

    printf("\t\tDest Port:  ");
    print_tcp_udp_port(&packet);
}

/*
Function: process_tcp_hdr
Input: current packet pointer, beginning of IP addresses for tcp psuedoheader, calculated tcp pdu lenght, 
            IP protocol for psuedoheader
Output: none

Methodology:
    - using our ideology of just advancing a pointer to get the bytes we need
    - passing the packet pointer into our get_short and get_long utility 
        functions to get, well, the shorts and longs we need
    - use pointer arithmatic to build the psuedoheader and checksum
*/
void process_tcp_hdr(const unsigned char* packet,const unsigned char* tcp_st, uint16_t tcp_pdu_len, char ip_ptcl){
    const unsigned char* tcp_begin = packet; //save this position for later to use in the checksum

    //get those ports!
    printf("\tTCP Header\n");
    printf("\t\tSource Port:  ");
    print_tcp_udp_port(&packet);
    printf("\t\tDest Port:  ");
    print_tcp_udp_port(&packet);

    printf("\t\tSequence Number: %u\n", get_long(&packet,1));
    
    printf("\t\tACK Number: %u\n", get_long(&packet,1));
    printf("\t\tData Offset (bytes): %i\n", ((packet++[0]>>4)&0xf)*4);
    
    //for the flags we just needs to mask off the bits we need
    char ack_f = packet[0] & 0x10;
    char rst_f = packet[0] & 0x4;
    char syn_f = packet[0] & 0x2;
    char fin_f = packet[0] & 0x1;

    printf("\t\tSYN Flag: %s\n", (syn_f)? "Yes" : "No");
    printf("\t\tRST Flag: %s\n", (rst_f)? "Yes" : "No");
    printf("\t\tFIN Flag: %s\n", (fin_f)? "Yes" : "No");
    printf("\t\tACK Flag: %s\n", (ack_f)? "Yes" : "No"); //anything greather than 0 is true :)
    packet++;

    printf("\t\tWindow Size: %i\n", get_short(&packet, 1));

    //time to build the psuedoheader (follow the diagram)
    uint8_t p_hdr[TCP_P_HDR_LEN] = {0};
    tcp_pdu_len = htons(tcp_pdu_len);
    memcpy(p_hdr, tcp_st, 2*IP_ADDR_SIZE);
    memcpy(p_hdr+2*IP_ADDR_SIZE+1, &ip_ptcl, 1);
    memcpy(p_hdr+2*IP_ADDR_SIZE+2, &tcp_pdu_len, 2);
    tcp_pdu_len = ntohs(tcp_pdu_len);

    //build the checksum (psuedoheader is known length)
    uint8_t tcp_chksm[TCP_P_HDR_LEN+tcp_pdu_len];
    memcpy(tcp_chksm, p_hdr, TCP_P_HDR_LEN);
    memcpy(tcp_chksm+TCP_P_HDR_LEN, tcp_begin, tcp_pdu_len);

    //check to see if we messed up our checksum
    printf("\t\tChecksum: ");
    if(in_cksum((unsigned short*)tcp_chksm, TCP_P_HDR_LEN+tcp_pdu_len) == 0){
        printf("Correct ");
    }else{
        printf("Incorrect ");
    }
    printf("(0x%04x)\n", get_short(&packet, 1));
}

//as with TCP header we will use the same methodology to parse the IP header
//this time using our print_ip() function to easily print the ip
void process_ip_hdr(const unsigned char* packet){
    const unsigned char* ip_begin = packet;
    printf("\tIP Header\n");

    uint16_t ip_vers = ((packet[0]>>4)&0xf);
    printf("\t\tIP Version: %i\n", ip_vers);

    uint16_t hdr_len = (packet++[0]&0xf)*4;//grab only the lower bit for the hdr_len

    printf("\t\tHeader Len (bytes): %i\n", hdr_len);

    uint8_t tos = packet++[0];
    uint8_t diffserv = tos>>2;
    uint8_t ecn = tos & 0x3;
    printf("\t\tTOS subfields:\n");
    printf("\t\t\tDiffserv bits: %i\n", diffserv);
    printf("\t\t\tECN bits: %i\n", ecn);

    uint16_t pdu_len = get_short(&packet, 1);
    packet = packet+LONG_BYTES;

    printf("\t\tTTL: %i\n", packet++[0]);
    printf("\t\tProtocol: ");

    char protocol = packet++[0];
    switch(protocol){
        case 1:
            printf("ICMP\n");
            break;
        case 6:
            printf("TCP\n");
            break;
        case 17:
            printf("UDP\n");
            break;
        default:
            printf("Unknown\n");
            break;
    }

    printf("\t\tChecksum: ");
    if(in_cksum((unsigned short*)ip_begin, hdr_len)==0){
        printf("Correct ");
    }else{
        printf("Incorrect ");
    }
    printf("(0x%04x)\n", get_short(&packet, 1)); // this checksum doesn't need to be converted from net to host :)

    const unsigned char* tcp_begin = packet; //save our spot in the packet for use in the TCP psuedoheader

    printf("\t\tSender IP: ");
    print_ip(&packet);
    printf("\t\tDest IP: ");
    print_ip(&packet);

    switch(protocol){
        case 1:
            printf("\n");
            process_icmp_hdr(ip_begin+hdr_len);
            break;
        case 6:
            printf("\n");
            process_tcp_hdr(ip_begin+hdr_len, tcp_begin, pdu_len - hdr_len, protocol);
            break;
        case 17:
            printf("\n");
            process_udp_hdr(ip_begin+hdr_len);
            break;
        default:
            break;
    }
}

//arp header processing is pretty mundane compared to the other ones
void process_arp_hdr(const unsigned char* packet){
    printf("\tARP header\n");

    printf("\t\tOpcode: ");

    packet = packet + (2*SHORT_BYTES) + 2;  //skip over these bytes i've separated them like this because this is
                                            //how the data is grouped in the packet

    uint16_t opcode = get_short(&packet, 1);
    switch(opcode){
        case 1:
            printf("Request\n");
            break;
        case 2:
            printf("Reply\n");
            break;
        default:
            printf("%i\n", opcode);
            break;
    }


    //all together now mac and ip!
    printf("\t\tSender MAC: ");
    print_mac(&packet);

    printf("\t\tSender IP: ");
    print_ip(&packet);

    printf("\t\tTarget MAC: ");
    print_mac(&packet);

    printf("\t\tTarget IP: ");
    print_ip(&packet);

    printf("\n");
}

void process_eth_hdr(const unsigned char* packet){
    printf("\tEthernet Header\n");

    printf("\t\tDest MAC: ");
    print_mac(&packet);

    printf("\t\tSource MAC: ");
    print_mac(&packet);

    printf("\t\tType: ");

    uint16_t type = get_short(&packet, 1);

    if(type == 0x0800){
        printf("IP\n\n");
        process_ip_hdr(packet);
    }else if(type == 0x0806){
        printf("ARP\n\n");
        process_arp_hdr(packet);   
    }else{
        printf("Unknown\n");
    }
    return;
}

int main(int argc, char* argv[]){
    //verify an argument was given
    if(argc != 2){
        fprintf(stderr, "Usage trace <File Name>\n");
        return -1;
    }
    char* fileName = argv[1];
    
    pcap_t* file;
    char p_o_errbuf[256];
    
    //verfiy the pcap file exists
    if((file = pcap_open_offline(fileName, p_o_errbuf)) == NULL){
        fprintf(stderr, "%s", p_o_errbuf);
        return -1;
    }

    //begin going through the packet
    struct pcap_pkthdr* pkthdr;
    const unsigned char* packet;
    int i = 1;

    while(pcap_next_ex(file, &pkthdr, &packet) > 0){
        printf("\nPacket number: %i  Packet Len: %i\n\n", i, pkthdr->len);
        process_eth_hdr(packet);
        i++;
    }

    pcap_close(file);
    return 0;
}
