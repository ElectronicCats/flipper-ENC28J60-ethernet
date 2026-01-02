#include "os_detector_module.h"

#include "app_user.h"

#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "tcp_module.h"

#define OPTS_LEN    13
#define OPTS_PROBES 6

/* 8 options:
 *  0~5: six options for SEQ/OPS/WIN/T1 probes.
 *  6:   ECN probe.
 *  7-12:   T2~T7 probes.
 *
 * option 0: WScale (10), Nop, MSS (1460), Timestamp, SackP
 * option 1: MSS (1400), WScale (0), SackP, T(0xFFFFFFFF,0x0), EOL
 * option 2: T(0xFFFFFFFF, 0x0), Nop, Nop, WScale (5), Nop, MSS (640)
 * option 3: SackP, T(0xFFFFFFFF,0x0), WScale (10), EOL
 * option 4: MSS (536), SackP, T(0xFFFFFFFF,0x0), WScale (10), EOL
 * option 5: MSS (265), SackP, T(0xFFFFFFFF,0x0)
 * option 6: WScale (10), Nop, MSS (1460), SackP, Nop, Nop
 * option 7-11: WScale (10), Nop, MSS (265), T(0xFFFFFFFF,0x0), SackP
 * option 12: WScale (15), Nop, MSS (265), T(0xFFFFFFFF,0x0), SackP
 */
static struct {
    uint8_t* val;
    uint16_t len;
} prbOpts[OPTS_LEN] = {
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x05\xb4\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x02\x04\x05\x78\x03\x03\x00\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x00",
     20},
    {(uint8_t*)"\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x01\x01\x03\x03\x05\x01\x02\x04\x02\x80",
     20},
    {(uint8_t*)"\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x03\x03\x0A\x00", 16},
    {(uint8_t*)"\x02\x04\x02\x18\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x03\x03\x0A\x00",
     20},
    {(uint8_t*)"\x02\x04\x01\x09\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00", 16},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x05\xb4\x04\x02\x01\x01", 12},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0f\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20}};

/* TCP Window sizes. Numbering is the same as for prbOpts[] */
uint16_t prbWindowSz[] = {1, 63, 4, 4, 16, 512, 3, 128, 256, 1024, 31337, 32768, 65535};

uint8_t seq_act = OFP_UNSET;

void ofp_tseq(App* app, uint8_t* target_ip);

void doSeqTests(App* app, uint8_t* target_ip) {
    switch(seq_act) {
    case OFP_TSEQ:
        ofp_tseq(app, target_ip);
        break;
    }
}

/*void make_tseq_packet(
    uint8_t* source_mac,
    uint8_t* target_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint8_t* source_port,
    uint8_t* target_port) {
    set_tcp_header_tseq(
        app->ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
        app->ethernet->ip_address,
        target_ip,
        source_port + 1,
        target_port,
        sequence,
        ack_number,
        prbWindowSz[i],
        0,
        &prbOpts[i].len,
        prbOpts[i].val,
        &tcp_len);

    set_ipv4_header(
        app->ethernet->tx_buffer + ETHERNET_HEADER_LEN,
        6, // Protocolo TCP
        tcp_len,
        app->ethernet->ip_address,
        target_ip);

    set_ethernet_header(app->ethernet->tx_buffer, app->ethernet->mac_address, target_mac, 0x0800);

    //send_packet(app->ethernet, app->ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

    printf("BUFFER: ");
    for(uint16_t j = 0; j < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); j++) {
        printf(
            "%02X%c",
            app->ethernet->tx_buffer[j],
            j == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }
}*/

void ofp_tseq(App* app, uint8_t* target_ip) {
    printf("IMPRIMIR OPCIONES:\n");
    for(uint8_t i = 0; i < OPTS_LEN; i++) {
        printf("OPTIONS: %u: %u -> %u ", i, prbOpts[i].len, prbWindowSz[i]);
        for(uint8_t j = 0; j < prbOpts[i].len; j++) {
            printf("%02X%c", prbOpts[i].val[j], j == (prbOpts[i].len - 1) ? '\n' : ' ');
        }
    }

    UNUSED(target_ip);
    uint8_t source_mac[6] = {0x00, 0xE0, 0x4C, 0x68, 0x0E, 0xC5};
    uint8_t target_mac[6] = {0xCC, 0x73, 0x14, 0x17, 0xA3, 0x45};
    uint8_t source_ip[4] = {192, 168, 0, 105};
    uint8_t target_ip_debug[4] = {192, 168, 0, 103};
    uint16_t ip_id[6] = {63073, 63326, 35094, 25039, 36881, 19500};
    uint16_t ip_flags_offset = 0;
    uint8_t ttl_vec[6] = {55, 51, 38, 52, 50, 52};
    uint16_t source_port = 63954;
    uint16_t target_port = 2121;
    uint32_t sequence = 1354199039;
    uint32_t ack_number = 64547392;

    uint16_t tcp_len = 0;

    /*arp_get_specific_mac(
        app->ethernet,
        app->ethernet->ip_address,
        (*(uint32_t*)app->ethernet->ip_address & *(uint32_t*)app->ethernet->subnet_mask) ==
                (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
            target_ip :
            app->ip_gateway,
        app->ethernet->mac_address,
        target_mac);*/

    for(uint8_t i = 0; i < OPTS_PROBES; i++) {
        printf("No. SEQ: %lu -> %04lx\n", sequence + i, sequence + i);

        set_tcp_header_tseq(
            app->ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
            source_ip,
            target_ip_debug,
            source_port + i,
            target_port,
            sequence + i,
            ack_number,
            prbWindowSz[i],
            0,
            &(prbOpts[i].len),
            prbOpts[i].val,
            &tcp_len);

        set_ipv4_header(
            app->ethernet->tx_buffer + ETHERNET_HEADER_LEN,
            6,
            tcp_len,
            source_ip,
            target_ip_debug,
            ip_id[i],
            ip_flags_offset,
            ttl_vec[i]);

        set_ethernet_header(app->ethernet->tx_buffer, source_mac, target_mac, 0x0800);

        //send_packet(app->ethernet, app->ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

        printf("BUFFER: ");
        for(uint16_t j = 0; j < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); j++) {
            printf(
                "%02x%c",
                app->ethernet->tx_buffer[j],
                j == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
        }

        uint32_t sequences_vector[6] = {0};
        uint16_t len_receive = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);
        UNUSED(len_receive);
        if(is_tcp(app->ethernet->rx_buffer)) {
            tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);
            bytes_to_uint(sequences_vector + i, tcp_header.sequence, sizeof(uint32_t));
        }
    }
}

void os_scan(void* context, uint8_t* target_ip) {
    App* app = context;

    seq_act = OFP_TSEQ;

    doSeqTests(app, target_ip);
}
