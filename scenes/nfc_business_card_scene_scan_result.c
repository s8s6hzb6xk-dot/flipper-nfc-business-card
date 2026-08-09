#include "../nfc_business_card_i.h"

static void nfc_business_card_scene_scan_result_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcBusinessCard* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcBusinessCardCustomEventSave);
    }
}

static void nfc_business_card_scene_scan_result_popup_callback(void* context) {
    NfcBusinessCard* app = context;
    scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, NfcBusinessCardSceneStart);
}

void nfc_business_card_scene_scan_result_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    nfc_business_card_format_contact(&app->scanned_card, app->tmp);

    widget_add_text_scroll_element(widget, 0, 0, 128, 50, furi_string_get_cstr(app->tmp));
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        "Save",
        nfc_business_card_scene_scan_result_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
}

bool nfc_business_card_scene_scan_result_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcBusinessCardCustomEventSave) {
        nfc_business_card_contact_path(app, &app->scanned_card, app->file_path);
        bool saved = business_card_save_vcf(
            &app->scanned_card, app->storage, furi_string_get_cstr(app->file_path));

        /* Popup keeps the pointer it is given, so park the name somewhere stable. */
        business_card_display_name(&app->scanned_card, app->tmp);
        size_t i = 0;
        const char* name = furi_string_get_cstr(app->tmp);
        while(name[i] != '\0' && i < sizeof(app->text_buffer) - 1) {
            app->text_buffer[i] = name[i];
            i++;
        }
        app->text_buffer[i] = '\0';

        popup_reset(app->popup);
        popup_set_header(
            app->popup, saved ? "Saved" : "Save failed", 64, 16, AlignCenter, AlignTop);
        popup_set_text(app->popup, app->text_buffer, 64, 34, AlignCenter, AlignTop);
        popup_set_context(app->popup, app);
        popup_set_callback(app->popup, nfc_business_card_scene_scan_result_popup_callback);
        popup_set_timeout(app->popup, 1500);
        popup_enable_timeout(app->popup);

        view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewPopup);
        consumed = true;
    }

    return consumed;
}

void nfc_business_card_scene_scan_result_on_exit(void* context) {
    NfcBusinessCard* app = context;
    widget_reset(app->widget);
    popup_reset(app->popup);
}
