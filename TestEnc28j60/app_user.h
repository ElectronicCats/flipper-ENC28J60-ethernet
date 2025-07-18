#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>

#include <dialogs/dialogs.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include "scenes_config/app_scene_functions.h"
#include <testing_app_icons.h>

#include "libraries/chip/enc28j60.h"
#include "modules/dhcp_protocol.h"

// Struct for the App
typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;

    FuriString* text; // String for general use
    FuriThread* thread; // For the threads
} App;

// Views in the App
typedef enum {
    WidgetView
} scenesViews;

// This functions works only to draw repetitive views in widgets
void draw_in_development(App* app); // draws when something is on development
void draw_device_no_connected(App* app); // draws when the device is not connected
void draw_network_not_connected(App* app); // draws if the device is not connected to a network
void draw_waiting_for_ip(App* app); // Draw when you're waiting for an IP
void draw_your_ip_is(App* app, uint8_t* ip); // Draw the IP when you got it
void draw_ip_not_got_it(App* app, uint8_t* ip); // Draw when get the ip failed
void draw_dora_failed(App* app); // Draw when the DORA process failed
