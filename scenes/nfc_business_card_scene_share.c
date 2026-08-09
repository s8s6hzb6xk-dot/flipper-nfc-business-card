#include "../nfc_business_card_i.h"

/**
 * The listener answers the reader entirely from the tag image we handed it, so
 * there is nothing to do per event beyond letting it run.
 */
static NfcCommand
    nfc_business_card_scene_share_listener_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return NfcCommandContinue;
}

void nfc_business_card_scene_share_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Widget* widget = app->widget;
    FuriString* text = app->tmp;

    widget_reset(widget);

    CardTagInfo info;
    card_tag_info(&app->my_card, &info);

    if(!info.fits || !card_tag_build(&app->my_card, app->nfc_device)) {
        nfc_business_card_show_card_error(app, &info, "Nothing to share");
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
        return;
    }

    business_card_display_name(&app->my_card, text);
    widget_add_string_element(
        widget, 64, 3, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(text));
    widget_add_string_element(
        widget, 64, 24, AlignCenter, AlignTop, FontSecondary, "Hold a phone against");
    widget_add_string_element(
        widget, 64, 35, AlignCenter, AlignTop, FontSecondary, "the back of the Flipper");

    furi_string_printf(
        text, "%s  %u/%u B", info.tag_name, (unsigned)info.payload_size, (unsigned)info.capacity);
    widget_add_string_element(
        widget, 64, 52, AlignCenter, AlignTop, FontSecondary, furi_string_get_cstr(text));

    app->listener = nfc_listener_alloc(
        app->nfc,
        NfcProtocolMfUltralight,
        nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight));
    nfc_listener_start(app->listener, nfc_business_card_scene_share_listener_callback, app);

    nfc_business_card_blink_emulate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
}

bool nfc_business_card_scene_share_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_business_card_scene_share_on_exit(void* context) {
    NfcBusinessCard* app = context;

    if(app->listener != NULL) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
        nfc_business_card_blink_stop(app);
    }

    widget_reset(app->widget);
}
