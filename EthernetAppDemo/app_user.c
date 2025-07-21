#include "app_user.h"

// Just to set as initial MAC the user must to modify to have other MAC address
uint8_t MAC_INITIAL[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d};
uint8_t IP_DEFAULT[4] = {192, 168, 0, 2};

// Function to make paths
void make_paths(App* app) {
    furi_assert(app);

    if(!storage_simply_mkdir(app->storage, PATHAPPEXT)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\napp folder");
    }
    if(!storage_simply_mkdir(app->storage, PATHPCAPS)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\nlogs folder");
    }
}

static bool app_scene_costum_callback(void* context, uint32_t costum_event) {
    furi_assert(context);
    App* app = (App*)context;
    return scene_manager_handle_custom_event(app->scene_manager, costum_event);
}

static bool app_scene_back_event(void* context) {
    furi_assert(context);
    App* app = (App*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void app_tick_event(void* context) {
    furi_assert(context);
    App* app = (App*)context;
    UNUSED(app);
}

App* app_alloc() {
    // Alloc the app memory
    App* app = (App*)malloc(sizeof(App));

    // Alloc the scene manager and view dispatcher
    app->scene_manager = scene_manager_alloc(&app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    // Set the navegation on the view dispatcher
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_scene_costum_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_scene_back_event);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, app_tick_event, 100);

    // Alloc the GUI Modules and add the view in the view dispatcher
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WidgetView, widget_get_view(app->widget));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SubmenuView, submenu_get_view(app->submenu));

    app->varList = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VarListView, variable_item_list_get_view(app->varList));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TextBoxView, text_box_get_view(app->text_box));

    app->input_byte_value = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, InputByteView, byte_input_get_view(app->input_byte_value));

    app->path = furi_string_alloc();

    app->file_browser = file_browser_alloc(app->path);
    view_dispatcher_add_view(
        app->view_dispatcher, FileBrowserView, file_browser_get_view(app->file_browser));

    app->text = furi_string_alloc();

    // Init the storage
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Alloc the file storage
    app->file = storage_file_alloc(app->storage);

    // Alloc the memory for the enc28j60 instance
    app->ethernet = enc28j60_alloc(MAC_INITIAL, IP_DEFAULT);

    make_paths(app);

    enc28j60_soft_reset(app->ethernet); // Soft reset the enc28j60
    app->enc28j60_connected = enc28j60_start(app->ethernet) !=
                              0xff; // To know if the enc28j60 is connected

    return app;
}

void app_free(App* app) {
    //  Free all the views from the View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, SubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, WidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, TextBoxView);
    view_dispatcher_remove_view(app->view_dispatcher, VarListView);
    view_dispatcher_remove_view(app->view_dispatcher, InputByteView);
    view_dispatcher_remove_view(app->view_dispatcher, FileBrowserView);

    // Free memory of Scene Manager and View Dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Free memory of GUI modules
    widget_free(app->widget);
    submenu_free(app->submenu);
    text_box_free(app->text_box);
    byte_input_free(app->input_byte_value);
    file_browser_free(app->file_browser);

    // Free memory of ENC
    free_enc28j60(app->ethernet);

    // Free memory of the text
    furi_string_free(app->text);
    furi_string_free(app->path);

    // Free the file storage
    storage_file_free(app->file);

    // Close records
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int app_main(void* p) {
    UNUSED(p);

    App* app = app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, app_scene_main_menu_option);

    view_dispatcher_run(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    app_free(app);

    return 0;
}
