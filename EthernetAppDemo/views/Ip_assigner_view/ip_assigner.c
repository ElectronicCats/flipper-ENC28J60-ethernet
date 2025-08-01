#include "ip_assigner.h"
#include "ethernet_app_icons.h"

//

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
static void my_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);

    UNUSED(context);

    // // Draw header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 10, "TEMPLATE");

    // Position of the rectangles
    uint8_t cell_pos_x = 2;
    uint8_t cell_pos_y = 30;

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

    // Selector position
    uint8_t selector_position_x = cell_pos_x + 2;
    uint8_t selector_position_y = cell_pos_y - 2;

    // Draw an icon
    canvas_draw_icon(canvas, cell_pos_x, cell_pos_y, &I_Ip_Selector_Template);

    // Draw the position numbers
    canvas_draw_icon(canvas, cell_one_first_digit, pos_y_digit_numbers, numbers_icons[1]);
    canvas_draw_icon(canvas, cell_one_second_digit, pos_y_digit_numbers, numbers_icons[9]);
    canvas_draw_icon(canvas, cell_one_third_digit, pos_y_digit_numbers, numbers_icons[2]);

    // Draw position
    canvas_draw_icon(canvas, cell_two_first_digit, pos_y_digit_numbers, numbers_icons[1]);
    canvas_draw_icon(canvas, cell_two_second_digit, pos_y_digit_numbers, numbers_icons[6]);
    canvas_draw_icon(canvas, cell_two_third_digit, pos_y_digit_numbers, numbers_icons[8]);

    // Draw position
    canvas_draw_icon(canvas, cell_three_first_digit, pos_y_digit_numbers, numbers_icons[0]);
    canvas_draw_icon(canvas, cell_three_second_digit, pos_y_digit_numbers, numbers_icons[0]);
    canvas_draw_icon(canvas, cell_three_third_digit, pos_y_digit_numbers, numbers_icons[0]);

    // Draw position
    canvas_draw_icon(canvas, cell_fourth_first_digit, pos_y_digit_numbers, numbers_icons[0]);
    canvas_draw_icon(canvas, cell_fourth_second_digit, pos_y_digit_numbers, numbers_icons[0]);
    canvas_draw_icon(canvas, cell_fourth_third_digit, pos_y_digit_numbers, numbers_icons[1]);

    // Draw selector
    // canvas_draw_box(canvas, selector_position_x, selector_position_y, 9, 17);
    draw_my_box(canvas, selector_position_x, selector_position_y);
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

    View* view = view_alloc();

    // Set the orientation
    view_set_orientation(view, ViewOrientationHorizontal);

    // Set input events, to get buttons events
    view_set_input_callback(view, input_callback);

    // Set the draw callback for the view
    view_set_draw_callback(view, my_draw_callback);

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
    view_free(instance->view);
    free(instance);
}

/**
 * Function to get the view
 */
View* ip_assigner_get_view(ip_assigner_t* instance) {
    return instance->view;
}
