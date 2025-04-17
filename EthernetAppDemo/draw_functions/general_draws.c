#include "../app_user.h"

// Draws a developmet
void draw_in_development(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 41, 0, &I_WIP45x38);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "WORK IN\nPROGRESS");
}

// Draws device not connected
void draw_device_no_connected(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 4, 0, &I_NOC119x38);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "DEVICE NOT\nCONNECTED");
}

// Draws if the network is not link
void draw_network_not_connected(App* app) {
    widget_reset(app->widget);
    // widget_add_icon_element(app->widget, 4, 0, &I_NOC119x38); Needs to change the icon
    widget_add_string_multiline_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Network\nNot Detected");
}
