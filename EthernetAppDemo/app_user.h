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
#include "ethernet_app_icons.h"

#include "libraries/chip/enc28j60.h"

#define APP_NAME    "ETHERNET APP"
#define APP_VERSION "BETA"

// Struct for the App
typedef struct {
    uint8_t mac_device[6];
    uint8_t ip_device[4];

    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* varList;
    TextBox* textBox;
    ByteInput* input_byte_value;
    // FileBrowser* file_browser;
    enc28j60_t* ethernet; // Instance for the enc28j60

    FuriString* text;
    FuriThread* thread;
} App;

// Views in the App
typedef enum {
    SubmenuView,
    WidgetView,
    VarListView,
    TextBoxView,
    DialogInfoView,
    InputByteView,
    // FileBrowserView,
} scenesViews;
