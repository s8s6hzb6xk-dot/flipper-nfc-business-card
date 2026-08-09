#include "../nfc_business_card_i.h"

static void nfc_business_card_scene_saved_detail_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcBusinessCard* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcBusinessCardCustomEventDelete);
    }
}

static void nfc_business_card_scene_saved_detail_popup_callback(void* context) {
    NfcBusinessCard* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void nfc_business_card_scene_saved_detail_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    nfc_business_card_format_contact(&app->scanned_card, app->tmp);

    widget_add_text_scroll_element(widget, 0, 0, 128, 50, furi_string_get_cstr(app->tmp));
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Delete",
        nfc_business_card_scene_saved_detail_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
}

bool nfc_business_card_scene_saved_detail_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcBusinessCardCustomEventDelete) {
        bool removed = storage_simply_remove(app->storage, furi_string_get_cstr(app->file_path));

        popup_reset(app->popup);
        popup_set_header(
            app->popup, removed ? "Deleted" : "Delete failed", 64, 26, AlignCenter, AlignCenter);
        popup_set_context(app->popup, app);
        popup_set_callback(app->popup, nfc_business_card_scene_saved_detail_popup_callback);
        popup_set_timeout(app->popup, 1200);
        popup_enable_timeout(app->popup);

        view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewPopup);
        consumed = true;
    }

    return consumed;
}

void nfc_business_card_scene_saved_detail_on_exit(void* context) {
    NfcBusinessCard* app = context;
    widget_reset(app->widget);
    popup_reset(app->popup);
}
