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

// Base timestamp reference (to be initialized on first use)
static uint32_t base_timestamp_sec = 0;
static uint32_t base_tick = 0;

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
    // Initialize timestamp reference
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    // Start with a base timestamp for the current date
    // This is an approximation since Flipper Zero doesn't have a full Unix timestamp
    // Using 2023-01-01 as arbitrary epoch for demonstration
    base_timestamp_sec =
        (uint32_t)(datetime.year - 2023) * 365 * 24 * 3600 + // Years (approximate)
        (uint32_t)(datetime.month * 30) * 24 * 3600 + // Months (approximate)
        (uint32_t)(datetime.day) * 24 * 3600 + // Days
        (uint32_t)(datetime.hour) * 3600 + // Hours
        (uint32_t)(datetime.minute) * 60 + // Minutes
        (uint32_t)(datetime.second); // Seconds

    // Store the current tick for relative time calculation
    base_tick = furi_get_tick();

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

    // Calculate time elapsed since base timestamp
    uint32_t current_tick = furi_get_tick();
    uint32_t elapsed_ticks = current_tick - base_tick;

    // Convert ticks to milliseconds (Flipper OS uses 1000 ticks per second)
    uint32_t elapsed_ms = elapsed_ticks;

    // Calculate seconds and microseconds
    uint32_t elapsed_sec = elapsed_ms / 1000;
    uint32_t elapsed_usec = (elapsed_ms % 1000) * 1000; // Convert remaining ms to us

    // Add to base timestamp
    packet_header.ts_sec = base_timestamp_sec + elapsed_sec;
    packet_header.ts_usec = elapsed_usec;

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
void pcap_close(File* file) {
    if(file) {
        storage_file_sync(file);
        storage_file_close(file);
    }
}

/**
 * From here the functions are to read de packets
 */

// Function to open pcap file and read the header
// Returns true if file is valid PCAP, false otherwise
size_t pcap_reader_init(File* file, const char* filename) {
    // Open file
    if(!storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return 0;
    }

    pcap_global_header_t header;
    size_t bytes_read = storage_file_read(file, &header, sizeof(header));

    // Check if we read the complete header and magic number is correct
    if(bytes_read != sizeof(header) || header.magic_number != 0xA1B2C3D4) {
        storage_file_close(file);
        return 0;
    }

    return bytes_read;
}

// Get the packet and return the size of the packet
size_t pcap_get_specific_packet(File* file, uint8_t* packet, uint32_t packet_position) {
    if(!file || !storage_file_is_open(file)) return 0;

    // Sync the file
    if(!storage_file_sync(file)) return 0;

    // Set the position on the file
    if(!storage_file_seek(file, packet_position, true)) return 0;

    pcap_packet_header_t packet_header;

    // Read packet header
    if(storage_file_read(file, &packet_header, sizeof(packet_header)) != sizeof(packet_header))
        return 0;

    // Read the packet data
    if(storage_file_read(file, packet, packet_header.orig_len) != packet_header.orig_len) return 0;

    // Return the actual bytes read
    return packet_header.orig_len;
}

// Get the position in the pcap file
uint32_t pcap_scan(File* file, const char* filename, uint64_t* positions) {
    // Counter and positions
    uint32_t counter = 0;

    // Get the first position
    if(!storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return 0;
    }

    // Get the total size of the file
    uint64_t file_size = storage_file_size(file);

    // This is for the pcap header field
    pcap_global_header_t header;

    // This part is to set the first position with bytes_read
    size_t bytes_read = storage_file_read(file, &header, sizeof(header));

    // Check if we read the complete header and magic number is correct
    if(bytes_read != sizeof(header) || header.magic_number != 0xA1B2C3D4) {
        storage_file_close(file);
        return 0;
    }

    // This variable will use to get the positions
    uint64_t position = 0;

    // Just for the packet
    uint8_t packet[1518] = {0};

    // For the packets
    pcap_packet_header_t packet_header;

    while(bytes_read > 0) {
        // Add the value for the position with bytes_read
        position = position + bytes_read;

        // Add the position
        positions[counter] = position;

        // Read the bytes to move the positions on the file, this line is for the packet header
        bytes_read = storage_file_read(file, &packet_header, sizeof(packet_header));

        // Pass the bytes on the file, this line is for the ethernet packet
        bytes_read += storage_file_read(file, &packet, packet_header.orig_len);

        // If the position has the same position as the total lenght of the file break the loop
        if(position == file_size) break;

        // Change the position on the array to save the positions on the file
        counter++;
    }

    storage_file_close(file);

    return counter;
}
