#include "../nfc_business_card_i.h"

#include <string.h>

static void nfc_business_card_scene_saved_list_submenu_callback(void* context, uint32_t index) {
    NfcBusinessCard* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/**
 * Walk the contacts folder once.
 *
 * Fills @p submenu when given one, and copies the name at position @p wanted
 * into @p found when given one. Returns the number of contacts on disk.
 */
static uint32_t nfc_business_card_scene_saved_list_walk(
    NfcBusinessCard* app,
    Submenu* submenu,
    uint32_t wanted,
    FuriString* found) {
    const size_t extension_len = strlen(NFC_BC_VCF_EXTENSION);
    File* dir = storage_file_alloc(app->storage);
    FuriString* label = furi_string_alloc();
    char name[128];
    uint32_t count = 0;

    if(storage_dir_open(dir, NFC_BC_CONTACTS_FOLDER)) {
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;

            size_t len = strlen(name);
            if(len <= extension_len) continue;
            if(strcmp(name + len - extension_len, NFC_BC_VCF_EXTENSION) != 0) continue;

            if(submenu != NULL) {
                furi_string_set_str(label, name);
                furi_string_left(label, len - extension_len);
                submenu_add_item(
                    submenu,
                    furi_string_get_cstr(label),
                    count,
                    nfc_business_card_scene_saved_list_submenu_callback,
                    app);
            }

            if(found != NULL && count == wanted) {
                furi_string_set_str(found, name);
            }

            count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_string_free(label);
    return count;
}

void nfc_business_card_scene_saved_list_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Saved Contacts");

    uint32_t count = nfc_business_card_scene_saved_list_walk(app, submenu, 0, NULL);

    if(count == 0) {
        Widget* widget = app->widget;
        widget_reset(widget);
        widget_add_string_element(
            widget, 64, 12, AlignCenter, AlignTop, FontPrimary, "No saved contacts");
        widget_add_text_box_element(
            widget,
            0,
            30,
            128,
            32,
            AlignCenter,
            AlignTop,
            "Scan a card and press Save\nto keep it here.",
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
        return;
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NfcBusinessCardSceneSavedList));
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewSubmenu);
}

bool nfc_business_card_scene_saved_list_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, NfcBusinessCardSceneSavedList, event.event);

        FuriString* name = furi_string_alloc();
        nfc_business_card_scene_saved_list_walk(app, NULL, event.event, name);

        if(furi_string_size(name) > 0) {
            furi_string_printf(
                app->file_path, "%s/%s", NFC_BC_CONTACTS_FOLDER, furi_string_get_cstr(name));

            if(business_card_load_vcf(
                   &app->scanned_card, app->storage, furi_string_get_cstr(app->file_path))) {
                scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneSavedDetail);
            }
        }

        furi_string_free(name);
        consumed = true;
    }

    return consumed;
}

void nfc_business_card_scene_saved_list_on_exit(void* context) {
    NfcBusinessCard* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
