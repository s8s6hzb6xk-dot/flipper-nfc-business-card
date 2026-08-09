#include "../nfc_business_card_i.h"

typedef enum {
    StartIndexShare,
    StartIndexScan,
    StartIndexWrite,
    StartIndexEdit,
    StartIndexSaved,
    StartIndexAbout,
} StartIndex;

static void nfc_business_card_scene_start_submenu_callback(void* context, uint32_t index) {
    NfcBusinessCard* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_business_card_scene_start_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "NFC Business Card");

    submenu_add_item(
        submenu,
        "Share My Card",
        StartIndexShare,
        nfc_business_card_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Scan a Card",
        StartIndexScan,
        nfc_business_card_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Write to Tag",
        StartIndexWrite,
        nfc_business_card_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Edit My Card",
        StartIndexEdit,
        nfc_business_card_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Saved Contacts",
        StartIndexSaved,
        nfc_business_card_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "About", StartIndexAbout, nfc_business_card_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NfcBusinessCardSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewSubmenu);
}

bool nfc_business_card_scene_start_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, NfcBusinessCardSceneStart, event.event);
        consumed = true;

        switch(event.event) {
        case StartIndexShare:
            /* Nothing to share yet — send the user straight to the editor. */
            if(business_card_is_empty(&app->my_card)) {
                scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneEditCard);
            } else {
                scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneShare);
            }
            break;
        case StartIndexScan:
            scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneScan);
            break;
        case StartIndexWrite:
            /* Same guard as sharing: an empty card has nothing to write. */
            if(business_card_is_empty(&app->my_card)) {
                scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneEditCard);
            } else {
                scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneWrite);
            }
            break;
        case StartIndexEdit:
            scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneEditCard);
            break;
        case StartIndexSaved:
            scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneSavedList);
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void nfc_business_card_scene_start_on_exit(void* context) {
    NfcBusinessCard* app = context;
    submenu_reset(app->submenu);
}
