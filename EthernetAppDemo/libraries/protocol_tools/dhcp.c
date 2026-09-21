#include "dhcp.h"
#include <stddef.h>

#define ETHERNET_HEADER_BYTES 14U
#define IPV4_MIN_HEADER_BYTES 20U
#define UDP_HEADER_BYTES      8U
#define DHCP_FIXED_BYTES      ((uint16_t)offsetof(dhcp_message_t, dhcp_options))

static uint16_t read_be16(const uint8_t* data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static bool dhcp_options_are_valid(const uint8_t* options, uint16_t options_len) {
    uint16_t pos = 0;
    while(pos < options_len) {
        uint8_t code = options[pos++];
        if(code == DHCP_PAD) continue;
        if(code == DHCP_END) return true;
        if(pos >= options_len) return false;

        uint8_t option_len = options[pos++];
        if(option_len > options_len - pos) return false;
        pos += option_len;
    }
    return false;
}

bool dhcp_message_discover(
    const uint8_t* mac_address,
    uint32_t xid,
    const uint8_t* host_name,
    dhcp_message_t* message,
    uint16_t* len) {
    if(!mac_address || !host_name || !message || !len) return false;

    memset(message, 0, sizeof(*message));
    message->operation = 1;
    message->htype = 1;
    message->hlen = 6;
    message->xid[0] = (xid >> 24) & 0xff;
    message->xid[1] = (xid >> 16) & 0xff;
    message->xid[2] = (xid >> 8) & 0xff;
    message->xid[3] = xid & 0xff;
    memcpy(message->chaddr, mac_address, 6);
    message->magic_cookie[0] = 0x63;
    message->magic_cookie[1] = 0x82;
    message->magic_cookie[2] = 0x53;
    message->magic_cookie[3] = 0x63;

    uint16_t size = 0;
    const uint8_t message_type[] = {DHCP_OP_DHCP_MESSAGE_TYPE, 1, DHCP_DISCOVER};
    memcpy(message->dhcp_options + size, message_type, sizeof(message_type));
    size += sizeof(message_type);

    uint8_t client_id[] = {DHCP_OP_CLIENT_IDENTIFIER, 7, 1, 0, 0, 0, 0, 0, 0};
    memcpy(client_id + 3, mac_address, 6);
    memcpy(message->dhcp_options + size, client_id, sizeof(client_id));
    size += sizeof(client_id);

    uint8_t host_option[] = {DHCP_OP_HOST_NAME, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(host_option + 2, host_name, 8);
    memcpy(message->dhcp_options + size, host_option, sizeof(host_option));
    size += sizeof(host_option);

    const uint8_t requested[] = {
        DHCP_OP_PARAMETER_REQUEST_LIST,
        3,
        DHCP_OP_SUBNET_MASK,
        DHCP_OP_ROUTER,
        DHCP_OP_DOMAIN_NAME_SERVER,
    };
    memcpy(message->dhcp_options + size, requested, sizeof(requested));
    size += sizeof(requested);
    message->dhcp_options[size++] = DHCP_END;

    *len = DHCP_FIXED_BYTES + size;
    return true;
}

bool dhcp_message_request(
    const uint8_t* mac_address,
    uint32_t xid,
    const uint8_t* ip_client,
    const uint8_t* ip_server,
    const uint8_t* host_name,
    dhcp_message_t* message,
    uint16_t* len) {
    if(!mac_address || !ip_client || !ip_server || !host_name || !message || !len) return false;

    memset(message, 0, sizeof(*message));
    message->operation = 1;
    message->htype = 1;
    message->hlen = 6;
    message->xid[0] = (xid >> 24) & 0xff;
    message->xid[1] = (xid >> 16) & 0xff;
    message->xid[2] = (xid >> 8) & 0xff;
    message->xid[3] = xid & 0xff;
    memcpy(message->chaddr, mac_address, 6);
    message->magic_cookie[0] = 0x63;
    message->magic_cookie[1] = 0x82;
    message->magic_cookie[2] = 0x53;
    message->magic_cookie[3] = 0x63;

    uint16_t size = 0;
    const uint8_t message_type[] = {DHCP_OP_DHCP_MESSAGE_TYPE, 1, DHCP_REQUEST};
    memcpy(message->dhcp_options + size, message_type, sizeof(message_type));
    size += sizeof(message_type);

    uint8_t client_id[] = {DHCP_OP_CLIENT_IDENTIFIER, 7, 1, 0, 0, 0, 0, 0, 0};
    memcpy(client_id + 3, mac_address, 6);
    memcpy(message->dhcp_options + size, client_id, sizeof(client_id));
    size += sizeof(client_id);

    uint8_t host_option[] = {DHCP_OP_HOST_NAME, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(host_option + 2, host_name, 8);
    memcpy(message->dhcp_options + size, host_option, sizeof(host_option));
    size += sizeof(host_option);

    uint8_t requested_ip[] = {DHCP_OP_REQUESTED_IP, 4, 0, 0, 0, 0};
    memcpy(requested_ip + 2, ip_client, 4);
    memcpy(message->dhcp_options + size, requested_ip, sizeof(requested_ip));
    size += sizeof(requested_ip);

    uint8_t server_id[] = {DHCP_OP_SERVER_IDENTIFIER, 4, 0, 0, 0, 0};
    memcpy(server_id + 2, ip_server, 4);
    memcpy(message->dhcp_options + size, server_id, sizeof(server_id));
    size += sizeof(server_id);

    const uint8_t requested[] = {
        DHCP_OP_PARAMETER_REQUEST_LIST,
        3,
        DHCP_OP_SUBNET_MASK,
        DHCP_OP_ROUTER,
        DHCP_OP_DOMAIN_NAME_SERVER,
    };
    memcpy(message->dhcp_options + size, requested, sizeof(requested));
    size += sizeof(requested);
    message->dhcp_options[size++] = DHCP_END;

    *len = DHCP_FIXED_BYTES + size;
    return true;
}

bool dhcp_parse_message(const uint8_t* frame, uint16_t frame_len, dhcp_message_view_t* message) {
    if(!frame || !message || frame_len < ETHERNET_HEADER_BYTES + IPV4_MIN_HEADER_BYTES) {
        return false;
    }
    if(frame[12] != 0x08 || frame[13] != 0x00) return false;

    const uint8_t* ip = frame + ETHERNET_HEADER_BYTES;
    if((ip[0] >> 4) != 4) return false;
    uint16_t ip_header_len = (ip[0] & 0x0fU) * 4U;
    if(ip_header_len < IPV4_MIN_HEADER_BYTES) return false;
    if(frame_len < ETHERNET_HEADER_BYTES + ip_header_len + UDP_HEADER_BYTES) return false;
    if(ip[9] != 0x11) return false;

    uint16_t ip_total_len = read_be16(ip + 2);
    if(ip_total_len < ip_header_len + UDP_HEADER_BYTES + DHCP_FIXED_BYTES) return false;
    if(ip_total_len > frame_len - ETHERNET_HEADER_BYTES) return false;

    const uint8_t* udp = ip + ip_header_len;
    uint16_t source_port = read_be16(udp);
    uint16_t destination_port = read_be16(udp + 2);
    if(!((source_port == 67 && destination_port == 68) ||
         (source_port == 68 && destination_port == 67))) {
        return false;
    }

    uint16_t udp_len = read_be16(udp + 4);
    if(udp_len < UDP_HEADER_BYTES + DHCP_FIXED_BYTES) return false;
    if(udp_len > ip_total_len - ip_header_len) return false;
    if(udp_len > frame_len - (uint16_t)(udp - frame)) return false;

    const uint8_t* dhcp = udp + UDP_HEADER_BYTES;
    uint16_t dhcp_len = udp_len - UDP_HEADER_BYTES;
    if(dhcp[236] != 0x63 || dhcp[237] != 0x82 || dhcp[238] != 0x53 || dhcp[239] != 0x63) {
        return false;
    }

    const uint8_t* options = dhcp + DHCP_FIXED_BYTES;
    uint16_t options_len = dhcp_len - DHCP_FIXED_BYTES;
    if(!dhcp_options_are_valid(options, options_len)) return false;

    message->data = dhcp;
    message->length = dhcp_len;
    message->options = options;
    message->options_length = options_len;
    return true;
}

bool dhcp_get_option(
    const dhcp_message_view_t* message,
    uint8_t option,
    const uint8_t** data,
    uint8_t* data_len) {
    if(!message || !data || !data_len) return false;

    uint16_t pos = 0;
    while(pos < message->options_length) {
        uint8_t code = message->options[pos++];
        if(code == DHCP_PAD) continue;
        if(code == DHCP_END) return false;
        if(pos >= message->options_length) return false;

        uint8_t option_len = message->options[pos++];
        if(option_len > message->options_length - pos) return false;
        if(code == option) {
            *data = message->options + pos;
            *data_len = option_len;
            return true;
        }
        pos += option_len;
    }
    return false;
}

static bool dhcp_is_type(const dhcp_message_view_t* message, uint8_t operation, uint8_t type) {
    if(!message || message->length < DHCP_FIXED_BYTES || message->data[0] != operation)
        return false;
    const uint8_t* data;
    uint8_t len;
    return dhcp_get_option(message, DHCP_OP_DHCP_MESSAGE_TYPE, &data, &len) && len == 1 &&
           data[0] == type;
}

bool dhcp_is_discover(const dhcp_message_view_t* message) {
    return dhcp_is_type(message, 1, DHCP_DISCOVER);
}

bool dhcp_is_offer(const dhcp_message_view_t* message) {
    return dhcp_is_type(message, 2, DHCP_OFFER);
}

bool dhcp_is_request(const dhcp_message_view_t* message) {
    return dhcp_is_type(message, 1, DHCP_REQUEST);
}

bool dhcp_is_acknoledge(const dhcp_message_view_t* message) {
    return dhcp_is_type(message, 2, DHCP_ACKNOLEDGE);
}

bool is_dhcp(const uint8_t* frame, uint16_t frame_len) {
    dhcp_message_view_t message;
    return dhcp_parse_message(frame, frame_len, &message);
}
