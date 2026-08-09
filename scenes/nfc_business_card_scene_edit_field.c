#include "../nfc_business_card_i.h"

#include <string.h>

static void nfc_business_card_scene_edit_field_callback(void* context) {
    NfcBusinessCard* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcBusinessCardCustomEventSave);
}

void nfc_business_card_scene_edit_field_on_enter(void* context) {
    NfcBusinessCard* app = context;

    /* Seed the keyboard with the value already stored. */
    const char* current = business_card_get(&app->my_card, app->edit_field);
    size_t i = 0;
    while(current[i] != '\0' && i < sizeof(app->text_buffer) - 1) {
        app->text_buffer[i] = current[i];
        i++;
    }
    app->text_buffer[i] = '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, business_card_field_label(app->edit_field));
    text_input_set_result_callback(
        app->text_input,
        nfc_business_card_scene_edit_field_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewTextInput);
}

bool nfc_business_card_scene_edit_field_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcBusinessCardCustomEventSave) {
        business_card_set(&app->my_card, app->edit_field, app->text_buffer);
        nfc_business_card_save_my_card(app);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_business_card_scene_edit_field_on_exit(void* context) {
    NfcBusinessCard* app = context;
    text_input_reset(app->text_input);
}
