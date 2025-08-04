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

#include "draw_functions/ip_assigner.h"
#include "scenes_config/app_scene_functions.h"
#include "ethernet_app_icons.h"

#include "libraries/chip/enc28j60.h"
#include "modules/arp_module.h"
#include "modules/dhcp_protocol.h"
#include "modules/tcp_module.h"
#include "modules/capture_module.h"
#include "modules/analysis_module.h"
#include "modules/ping_module.h"

// Version of the app
#define APP_NAME    "ETHERNET APP"
#define APP_VERSION "vBETA"

// Path for the files
#define PATHAPP    "apps_data/ethernet" // Path
#define PATHAPPEXT EXT_PATH(PATHAPP) // Add path to the Flipper

#define PATHPCAPS PATHAPPEXT "/files" // Path to save pcaps

// Create flags
typedef enum {
    flag_stop = 1,
    flag_dhcp_dora,
} ethernet_app_flags_t;

#define ALL_FLAGS (flag_stop | flag_dhcp_dora)

#define MASK_FLAGS 0xfffffffe

#define IS_NOT_LINK_UP 0xff

// For GET IP scene Events
typedef enum {
    wait_ip_event = 1,
    ip_no_gotten_event,
    ip_gotten_event,
} get_ip_events;

// Struct for the App
typedef struct {
    arp_list ip_list[255];
    uint8_t ip_counter; // Variable for countrt of ip_list
    uint8_t ip_gateway[4]; // Array to save the gateway ip
    uint8_t mac_gateway[6]; // Array to save the mac_gateway

    uint8_t ip_helper[4];
    uint8_t mac_helper[4];

    bool is_static_ip; // To know if the device has the static IP
    bool enc28j60_connected; // To know if the enc28j60 is connected

    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* varList;
    TextBox* text_box;
    ByteInput* input_byte_value;
    FileBrowser* file_browser;
    ip_assigner_t* ip_assigner;

    enc28j60_t* ethernet; // Instance for the enc28j60

    Storage* storage; // Set the storage
    DialogsApp* dialogs;

    File* file; // File to save logs

    FuriString* text; // String for general use
    FuriString* path; // String to get path from file browser

    FuriThread* thread; // For the threads
    FuriThread* thread_alternative; // For the threads
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
    IpAssignerView
} scenesViews;

// This functions works only to draw repetitive views in widgets
void draw_in_development(App* app); // draws when something is on development
void draw_device_no_connected(App* app); // draws when the device is not connected
void draw_network_not_connected(App* app); // draws if the device is not connected to a network
void draw_waiting_for_ip(App* app); // Draw when you're waiting for an IP
void draw_your_ip_is(App* app); // Draw the IP when you got it
void draw_ip_not_got_it(App* app); // Draw when get the ip failed
void draw_dora_failed(App* app); // Draw when the DORA process failed
void draw_ask_for_ip(App* app); // Draw to ask a new IP

// Thread
int32_t ethernet_thread(void* context);
