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
    widget_add_icon_element(app->widget, 41, 0, &I_NCR);
    widget_add_string_multiline_element(
        app->widget, 64, 60, AlignCenter, AlignBottom, FontPrimary, "Network\nNot Detected");
}

// Function to draw when is waiting for the IP of the DORA process
void draw_waiting_for_ip(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting for IP");
}

// Function to draw when a Ip is got it
void draw_your_ip_is(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Your IP is: ");

    furi_string_reset(app->text);

    furi_string_cat_printf(
        app->text,
        "%u:%u:%u:%u",
        app->ethernet->ip_address[0],
        app->ethernet->ip_address[1],
        app->ethernet->ip_address[2],
        app->ethernet->ip_address[3]);

    widget_add_string_element(
        app->widget,
        64,
        40,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

// Draw if you didnt got it the IP address
void draw_ip_not_got_it(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 15, AlignCenter, AlignCenter, FontPrimary, "IP didnt got it");
    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "IP by default: ");

    furi_string_reset(app->text);

    furi_string_cat_printf(
        app->text,
        "%u:%u:%u:%u",
        app->ethernet->ip_address[0],
        app->ethernet->ip_address[1],
        app->ethernet->ip_address[2],
        app->ethernet->ip_address[3]);

    widget_add_string_element(
        app->widget,
        64,
        40,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

void draw_dora_failed(App* app) {
    widget_reset(app->widget);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "DORA PROCESS FAILED");
    widget_add_string_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(app->text));
}
