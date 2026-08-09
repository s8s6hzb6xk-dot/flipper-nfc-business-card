#include "../nfc_business_card_i.h"

/** Menu index that follows the contact fields. */
#define EDIT_CARD_INDEX_SHARE_MODE (BusinessCardFieldCount)

static void nfc_business_card_scene_edit_card_submenu_callback(void* context, uint32_t index) {
    NfcBusinessCard* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void nfc_business_card_scene_edit_card_build(NfcBusinessCard* app, uint32_t selected) {
    Submenu* submenu = app->submenu;
    FuriString* label = app->tmp;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Edit My Card");

    for(size_t i = 0; i < BusinessCardFieldCount; i++) {
        const char* value = business_card_get(&app->my_card, i);
        furi_string_printf(
            label, "%s: %s", business_card_field_label(i), (value[0] != '\0') ? value : "-");
        submenu_add_item(
            submenu,
            furi_string_get_cstr(label),
            i,
            nfc_business_card_scene_edit_card_submenu_callback,
            app);
    }

    furi_string_printf(
        label, "Share as: %s", business_card_share_mode_label(app->my_card.share_mode));
    submenu_add_item(
        submenu,
        furi_string_get_cstr(label),
        EDIT_CARD_INDEX_SHARE_MODE,
        nfc_business_card_scene_edit_card_submenu_callback,
        app);

    submenu_set_selected_item(submenu, selected);
}

void nfc_business_card_scene_edit_card_on_enter(void* context) {
    NfcBusinessCard* app = context;

    nfc_business_card_scene_edit_card_build(
        app, scene_manager_get_scene_state(app->scene_manager, NfcBusinessCardSceneEditCard));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewSubmenu);
}

bool nfc_business_card_scene_edit_card_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, NfcBusinessCardSceneEditCard, event.event);
        consumed = true;

        if(event.event < BusinessCardFieldCount) {
            app->edit_field = (BusinessCardField)event.event;
            scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneEditField);

        } else if(event.event == EDIT_CARD_INDEX_SHARE_MODE) {
            /* Cycle through the record layouts in place. */
            app->my_card.share_mode = (app->my_card.share_mode + 1) % BusinessCardShareCount;
            nfc_business_card_save_my_card(app);
            nfc_business_card_scene_edit_card_build(app, EDIT_CARD_INDEX_SHARE_MODE);

        } else {
            consumed = false;
        }
    }

    return consumed;
}

void nfc_business_card_scene_edit_card_on_exit(void* context) {
    NfcBusinessCard* app = context;
    nfc_business_card_save_my_card(app);
    submenu_reset(app->submenu);
}
