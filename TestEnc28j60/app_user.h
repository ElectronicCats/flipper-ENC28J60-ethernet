#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

#include <dialogs/dialogs.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/file_browser_worker.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include "scenes_config/app_scene_functions.h"
#include <testing_app_icons.h>

#include "libraries/chip/enc28j60.h"
#include "modules/arp_module.h"
#include "modules/dhcp_protocol.h"
#include "modules/tcp_module.h"
#include "modules/capture_module.h"
#include "modules/analysis_module.h"

// Struct for the App
typedef struct {
    uint8_t mac_device[6];
    uint8_t ip_device[4];
    arp_list ip_list[255];

    bool enc28j60_connected; // To know if the enc28j60 is connected

    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* varList;
    TextBox* text_box;
    ByteInput* input_byte_value;
    FileBrowser* file_browser;
    enc28j60_t* ethernet; // Instance for the enc28j60

    Storage* storage; // Set the storage
    DialogsApp* dialogs;

    File* file; // File to save logs

    FuriString* text; // String for general use
    FuriString* path; // String to get path from file browser
    FuriThread* thread; // For the threads
} App;

// Views in the App
typedef enum {
    SubmenuView,
    WidgetView,
    VarListView,
    TextBoxView,
    DialogInfoView,
    InputByteView,
    FileBrowserView,
} scenesViews;

// This functions works only to draw repetitive views in widgets
void draw_in_development(App* app); // draws when something is on development
void draw_device_no_connected(App* app); // draws when the device is not connected
void draw_network_not_connected(App* app); // draws if the device is not connected to a network
void draw_waiting_for_ip(App* app); // Draw when you're waiting for an IP
void draw_your_ip_is(App* app); // Draw the IP when you got it
void draw_ip_not_got_it(App* app); // Draw when get the ip failed
void draw_dora_failed(App* app); // Draw when the DORA process failed