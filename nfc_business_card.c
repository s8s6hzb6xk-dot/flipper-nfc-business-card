#include "nfc_business_card_i.h"

static bool nfc_business_card_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcBusinessCard* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_business_card_back_event_callback(void* context) {
    furi_assert(context);
    NfcBusinessCard* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

bool nfc_business_card_save_my_card(NfcBusinessCard* app) {
    furi_assert(app);
    return business_card_save(&app->my_card, app->storage, NFC_BC_MY_CARD_PATH);
}

void nfc_business_card_blink_emulate(NfcBusinessCard* app) {
    notification_message(app->notifications, &sequence_blink_start_magenta);
}

void nfc_business_card_blink_scan(NfcBusinessCard* app) {
    notification_message(app->notifications, &sequence_blink_start_cyan);
}

void nfc_business_card_blink_stop(NfcBusinessCard* app) {
    notification_message(app->notifications, &sequence_blink_stop);
}

void nfc_business_card_format_contact(const BusinessCard* card, FuriString* out) {
    furi_assert(card);
    furi_assert(out);

    static const BusinessCardField order[] = {
        BusinessCardFieldTitle,
        BusinessCardFieldCompany,
        BusinessCardFieldPhone,
        BusinessCardFieldEmail,
        BusinessCardFieldUrl,
        BusinessCardFieldNote,
    };

    business_card_display_name(card, out);

    for(size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        const char* value = business_card_get(card, order[i]);
        if(value[0] == '\0') continue;
        furi_string_push_back(out, '\n');
        furi_string_cat_str(out, value);
    }
}

void nfc_business_card_show_card_error(
    NfcBusinessCard* app,
    const CardTagInfo* info,
    const char* title) {
    Widget* widget = app->widget;
    FuriString* text = app->tmp;

    widget_add_string_element(widget, 64, 4, AlignCenter, AlignTop, FontPrimary, title);

    if(info->payload_size == 0) {
        furi_string_set_str(
            text, "\"Share as\" is set to URL but\nno website is set on your card.");
    } else {
        furi_string_printf(
            text,
            "Your contact needs %u bytes,\nthe largest tag holds %u.\nShorten a field.",
            (unsigned)info->payload_size,
            (unsigned)info->capacity);
    }

    widget_add_text_box_element(
        widget, 0, 20, 128, 44, AlignCenter, AlignTop, furi_string_get_cstr(text), false);
}

void nfc_business_card_contact_path(
    NfcBusinessCard* app,
    const BusinessCard* card,
    FuriString* out) {
    furi_assert(app);

    FuriString* stem = furi_string_alloc();
    business_card_file_stem(card, stem);

    furi_string_printf(
        out, "%s/%s%s", NFC_BC_CONTACTS_FOLDER, furi_string_get_cstr(stem), NFC_BC_VCF_EXTENSION);

    /* Two people can share a name; never overwrite one with the other. */
    uint32_t suffix = 1;
    while(storage_file_exists(app->storage, furi_string_get_cstr(out)) && suffix < 1000) {
        furi_string_printf(
            out,
            "%s/%s_%lu%s",
            NFC_BC_CONTACTS_FOLDER,
            furi_string_get_cstr(stem),
            (unsigned long)suffix,
            NFC_BC_VCF_EXTENSION);
        suffix++;
    }

    furi_string_free(stem);
}

static NfcBusinessCard* nfc_business_card_alloc(void) {
    NfcBusinessCard* app = malloc(sizeof(NfcBusinessCard));

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nfc_business_card_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_business_card_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_business_card_back_event_callback);

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcBusinessCardViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcBusinessCardViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcBusinessCardViewWidget, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcBusinessCardViewPopup, popup_get_view(app->popup));

    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    app->scanned_data = mf_ultralight_alloc();
    app->listener = NULL;
    app->poller = NULL;

    app->file_path = furi_string_alloc();
    app->tmp = furi_string_alloc();

    business_card_reset(&app->my_card);
    business_card_reset(&app->scanned_card);

    /* Scanned contacts live next to the user's own card on the SD card. */
    storage_common_mkdir(app->storage, NFC_BC_BASE_FOLDER);
    storage_common_mkdir(app->storage, NFC_BC_APP_FOLDER);
    storage_common_mkdir(app->storage, NFC_BC_CONTACTS_FOLDER);

    business_card_load(&app->my_card, app->storage, NFC_BC_MY_CARD_PATH);

    return app;
}

static void nfc_business_card_free(NfcBusinessCard* app) {
    furi_assert(app);

    /* Scenes stop these on exit, but never leave the radio running. */
    if(app->listener != NULL) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
    }
    if(app->poller != NULL) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    view_dispatcher_remove_view(app->view_dispatcher, NfcBusinessCardViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, NfcBusinessCardViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, NfcBusinessCardViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, NfcBusinessCardViewPopup);
    popup_free(app->popup);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    mf_ultralight_free(app->scanned_data);
    nfc_device_free(app->nfc_device);
    nfc_free(app->nfc);

    furi_string_free(app->file_path);
    furi_string_free(app->tmp);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t nfc_business_card_app(void* p) {
    UNUSED(p);

    NfcBusinessCard* app = nfc_business_card_alloc();

    scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    nfc_business_card_free(app);
    return 0;
}
