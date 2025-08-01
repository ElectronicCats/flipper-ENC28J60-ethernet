#include "ip_assigner.h"
#include "ethernet_app_icons.h"

struct ip_assigner_t {
    View* view;
};

// struct for the model
typedef struct {
    uint8_t selector_position;
    FuriString* header;
    void* context;
    uint8_t* ip_array;
} ip_assigner_model;

// Array for the numbers
const Icon* numbers_icons[] = {
    &I_Number_0,
    &I_Number_1,
    &I_Number_2,
    &I_Number_3,
    &I_Number_4,
    &I_Number_5,
    &I_Number_6,
    &I_Number_7,
    &I_Number_8,
    &I_Number_9,
};

// Function to get the digit from a decimal number
static uint8_t get_num(uint8_t number, uint8_t digit_pos) {
    for(uint8_t i = 0; i < digit_pos; i++) {
        number = number / 10;
    }
    return number % 10;
}

// Get the digits for the any Ip number
static void get_digits(uint8_t number, uint8_t* digits) {
    digits[0] = get_num(number, 2); // get the first digit
    digits[1] = get_num(number, 1); // get the second digit
    digits[2] = get_num(number, 0); // get the third digit
}

// Draw the box of the selector
static void draw_my_box(Canvas* canvas, uint8_t pos_x, uint8_t pos_y) {
    // Draw Top side
    canvas_draw_line(canvas, pos_x, pos_y, pos_x + 8, pos_y);

    // Draw Left side
    canvas_draw_line(canvas, pos_x, pos_y + 16, pos_x, pos_y);

    // Draw Bottom side
    canvas_draw_line(canvas, pos_x, pos_y + 16, pos_x + 8, pos_y + 16);

    // Draw Bottom side
    canvas_draw_line(canvas, pos_x + 8, pos_y, pos_x + 8, pos_y + 16);

    int8_t reference_top_y = pos_y - 2;
    int8_t reference_top_x = pos_x + 2;

    // draw selection reference top
    canvas_draw_dot(canvas, reference_top_x + 2, reference_top_y - 2);
    canvas_draw_line(
        canvas, reference_top_x, reference_top_y, reference_top_x + 4, reference_top_y);
    canvas_draw_line(
        canvas, reference_top_x + 1, reference_top_y - 1, reference_top_x + 3, reference_top_y - 1);

    int8_t reference_bottom_y = pos_y + 18;
    int8_t reference_bottom_x = pos_x + 2;

    // draw selection reference bottom
    canvas_draw_dot(canvas, reference_bottom_x + 2, reference_bottom_y + 2);
    canvas_draw_line(
        canvas, reference_bottom_x, reference_bottom_y, reference_bottom_x + 4, reference_bottom_y);
    canvas_draw_line(
        canvas,
        reference_bottom_x + 1,
        reference_bottom_y + 1,
        reference_bottom_x + 3,
        reference_bottom_y + 1);
}

// This function Works to draw the hello world (by the moment)
static void my_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(_model);

    ip_assigner_model* model = (ip_assigner_model*)_model;

    if(!furi_string_empty(model->header)) {
        // Draw header
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 10, furi_string_get_cstr(model->header));
    }

    // Position of the rectangles
    uint8_t cell_pos_x = 2;
    uint8_t cell_pos_y = 30;

    // Draw positions
    canvas_draw_icon(canvas, cell_pos_x, cell_pos_y, &I_Ip_Selector_Template);

    // Y position of the digits
    uint8_t pos_y_digit_numbers = cell_pos_y + 2;

    // First digit position of the first three numbers
    uint8_t cell_one_first_digit = cell_pos_x + 4;
    uint8_t cell_one_second_digit = cell_one_first_digit + 9;
    uint8_t cell_one_third_digit = cell_one_second_digit + 9;

    // First digit position of the second three numbers
    uint8_t cell_two_first_digit = cell_pos_x + 34;
    uint8_t cell_two_second_digit = cell_two_first_digit + 9;
    uint8_t cell_two_third_digit = cell_two_second_digit + 9;

    // First digit position of the second three numbers
    uint8_t cell_three_first_digit = cell_pos_x + 66;
    uint8_t cell_three_second_digit = cell_three_first_digit + 9;
    uint8_t cell_three_third_digit = cell_three_second_digit + 9;

    // First digit position of the second three numbers
    uint8_t cell_fourth_first_digit = cell_pos_x + 98;
    uint8_t cell_fourth_second_digit = cell_fourth_first_digit + 9;
    uint8_t cell_fourth_third_digit = cell_fourth_second_digit + 9;

    if(model->ip_array != NULL) {
        // Get numbers
        uint8_t first_cell[3] = {0};
        uint8_t second_cell[3] = {0};
        uint8_t third_cell[3] = {0};
        uint8_t fourth_cell[3] = {0};

        // Set digits
        get_digits(model->ip_array[0], first_cell);
        get_digits(model->ip_array[1], second_cell);
        get_digits(model->ip_array[2], third_cell);
        get_digits(model->ip_array[3], fourth_cell);

        // Draw the position numbers
        canvas_draw_icon(
            canvas, cell_one_first_digit, pos_y_digit_numbers, numbers_icons[first_cell[0]]);
        canvas_draw_icon(
            canvas, cell_one_second_digit, pos_y_digit_numbers, numbers_icons[first_cell[1]]);
        canvas_draw_icon(
            canvas, cell_one_third_digit, pos_y_digit_numbers, numbers_icons[first_cell[2]]);

        // Draw position
        canvas_draw_icon(
            canvas, cell_two_first_digit, pos_y_digit_numbers, numbers_icons[second_cell[0]]);
        canvas_draw_icon(
            canvas, cell_two_second_digit, pos_y_digit_numbers, numbers_icons[second_cell[1]]);
        canvas_draw_icon(
            canvas, cell_two_third_digit, pos_y_digit_numbers, numbers_icons[second_cell[2]]);

        // Draw position
        canvas_draw_icon(
            canvas, cell_three_first_digit, pos_y_digit_numbers, numbers_icons[third_cell[0]]);
        canvas_draw_icon(
            canvas, cell_three_second_digit, pos_y_digit_numbers, numbers_icons[third_cell[1]]);
        canvas_draw_icon(
            canvas, cell_three_third_digit, pos_y_digit_numbers, numbers_icons[third_cell[2]]);

        // Draw position
        canvas_draw_icon(
            canvas, cell_fourth_first_digit, pos_y_digit_numbers, numbers_icons[fourth_cell[0]]);
        canvas_draw_icon(
            canvas, cell_fourth_second_digit, pos_y_digit_numbers, numbers_icons[fourth_cell[1]]);
        canvas_draw_icon(
            canvas, cell_fourth_third_digit, pos_y_digit_numbers, numbers_icons[fourth_cell[2]]);

        // Selector position
        uint8_t selector_position_x = cell_pos_x + 2;
        uint8_t selector_position_y = cell_pos_y - 2;

        // Draw an icon
        draw_my_box(canvas, selector_position_x, selector_position_y);
    }
}

// This function works for the inpust on the view
static bool input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    UNUSED(context);
    UNUSED(input_event);
    return false;
}

/**
 * Function to alloc the ip assigner 
 */
ip_assigner_t* ip_assigner_alloc() {
    ip_assigner_t* instance = (ip_assigner_t*)malloc(sizeof(ip_assigner_t));

    // Alloc view
    View* view = view_alloc();

    // Set the orientation
    view_set_orientation(view, ViewOrientationHorizontal);

    // Set the draw callback for the view
    view_set_draw_callback(view, my_draw_callback);

    // Set input events, to get buttons events
    view_set_input_callback(view, input_callback);

    // alloc model
    view_allocate_model(view, ViewModelTypeLocking, sizeof(ip_assigner_model));

    // Init the view model
    with_view_model(
        view,
        ip_assigner_model * model,
        {
            model->selector_position = 0;
            model->header = furi_string_alloc();
        },
        true);

    // Set the view
    instance->view = view;

    // Set the view context
    view_set_context(view, instance);

    return instance;
}

/**
 * Function to free the IP assigner 
 */
void ip_assigner_free(ip_assigner_t* instance) {
    with_view_model(
        instance->view, ip_assigner_model * model, { furi_string_free(model->header); }, true);
    view_free(instance->view);
    free(instance);
}

/**
 * Function to get the view
 */
View* ip_assigner_get_view(ip_assigner_t* instance) {
    return instance->view;
}

/**
 * Function set the header
 */
void ip_assigner_set_header(ip_assigner_t* instance, const char* text) {
    furi_check(instance);

    with_view_model(
        instance->view,
        ip_assigner_model * model,
        {
            if(text == NULL) {
                furi_string_reset(model->header);
            } else {
                furi_string_set_str(model->header, text);
            }
        },
        true);
}

/**
 * Set array ip in the model
 */
void ip_assigner_set_ip_array(ip_assigner_t* instance, uint8_t* ip_array) {
    furi_assert(instance);
    with_view_model(
        instance->view, ip_assigner_model * model, { model->ip_array = ip_array; }, true);
}
