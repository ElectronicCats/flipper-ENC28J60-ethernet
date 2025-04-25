#ifndef CAPTURE_MODULE_H_
#define CAPTURE_MODULE_H_

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

void create_pcap_name(FuriString* complete_path, const char* PATH, const char* name);

bool pcap_capture_init(File* file, const char* filename);

bool pcap_capture_add_packet(File* file, const uint8_t* packet, uint32_t packet_len);

bool pcap_capture_sync(File* file);

void pcap_capture_close(File* file);

#endif
