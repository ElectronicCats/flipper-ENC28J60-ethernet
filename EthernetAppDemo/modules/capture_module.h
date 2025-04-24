#ifndef CAPTURE_MODULE_H_
#define CAPTURE_MODULE_H_

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

void create_name(FuriString* complete_path, const char* PATH, const char* name);

bool pcap_capture_init(File* file, const char* filename);

#endif
