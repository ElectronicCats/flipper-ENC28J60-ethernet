#include "capture_module.h"

// PCAP global header structure
typedef struct pcap_global_header {
    uint32_t magic_number; // Magic number (0xA1B2C3D4 in big endian)
    uint16_t version_major; // Major version number (2)
    uint16_t version_minor; // Minor version number (4)
    int32_t thiszone; // GMT to local correction (0)
    uint32_t sigfigs; // Accuracy of timestamps (0)
    uint32_t snaplen; // Max length of captured packets
    uint32_t network; // Data link type (1 = Ethernet)
} pcap_global_header_t;

// PCAP packet header structure
typedef struct pcap_packet_header {
    uint32_t ts_sec; // Timestamp seconds
    uint32_t ts_usec; // Timestamp microseconds
    uint32_t incl_len; // Number of octets of packet saved in file
    uint32_t orig_len; // Actual length of packet
} pcap_packet_header_t;

// Function to create path name
void create_pcap_name(FuriString* complete_path, const char* PATH, const char* name) {
    furi_string_reset(complete_path);
    furi_string_cat_printf(complete_path, "%s", PATH);
    furi_string_cat_printf(complete_path, "/");
    furi_string_cat_printf(complete_path, "%s", name);
    furi_string_cat_printf(complete_path, ".pcap");
}

// Function to Initialize the pcap file
// The filename includes the path
bool pcap_capture_init(File* file, const char* filename) {
    // Create the file
    if(!storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        return false;
    }

    // Write PCAP header
    pcap_global_header_t header;
    header.magic_number = 0xA1B2C3D4;
    header.version_major = 2;
    header.version_minor = 4;
    header.thiszone = 0;
    header.sigfigs = 0;
    header.snaplen = 65535;
    header.network = 1; // Ethernet

    // Write header to file
    if(storage_file_write(file, &header, sizeof(header)) != sizeof(header)) {
        storage_file_close(file);
        return false;
    }

    // Flush to ensure header is written
    storage_file_sync(file);

    return true;
}

// Add the packet to the PCAP file
bool pcap_capture_add_packet(File* file, const uint8_t* packet, uint32_t packet_len) {
    // Create packet header
    pcap_packet_header_t packet_header;

    // Get real time instead of ticks for better compatibility
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    // Convert to Unix timestamp (seconds since Jan 1, 1970)
    // This is a simple approximation
    uint32_t timestamp_sec = datetime.hour * 3600 + datetime.minute * 60 + datetime.second;

    // Use milliseconds for microsecond field (better than nothing)
    uint32_t timestamp_usec = furi_get_tick() % 1000 * 1000;

    packet_header.ts_sec = (uint32_t)(timestamp_sec / 1000000);
    packet_header.ts_usec = (uint32_t)(timestamp_usec % 1000000);
    packet_header.incl_len = packet_len;
    packet_header.orig_len = packet_len;

    // Write packet header
    if(storage_file_write(file, &packet_header, sizeof(packet_header)) != sizeof(packet_header)) {
        return false;
    }

    // Write packet data
    if(storage_file_write(file, packet, packet_len) != packet_len) {
        return false;
    }

    return true;
}

// Sync the PCAP file to ensure data is written to storage
bool pcap_capture_sync(File* file) {
    if(!file) return false;
    return storage_file_sync(file);
}

// Close the PCAP file
void pcap_capture_close(File* file) {
    if(file) {
        storage_file_sync(file);
        storage_file_close(file);
    }
}
