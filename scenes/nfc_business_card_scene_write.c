#include "../nfc_business_card_i.h"

/**
 * Writes the same NDEF message the Share screen emulates onto a real blank
 * tag. The poller writes pages 4 upward, so the tag keeps its own UID and its
 * factory capability container — only the NDEF area is replaced.
 */
static NfcCommand
    nfc_business_card_scene_write_poller_callback(NfcGenericEvent event, void* context) {
    NfcBusinessCard* app = context;
    NfcCommand command = NfcCommandContinue;

    furi_assert(event.protocol == NfcProtocolMfUltralight);
    MfUltralightPollerEvent* mfu_event = event.event_data;

    switch(mfu_event->type) {
    case MfUltralightPollerEventTypeRequestMode:
        mfu_event->data->poller_mode = MfUltralightPollerModeWrite;
        break;

    case MfUltralightPollerEventTypeAuthRequest:
        mfu_event->data->auth_context.skip_auth = true;
        break;

    case MfUltralightPollerEventTypeRequestWriteData:
        mfu_event->data->write_data = app->write_data;
        break;

    case MfUltralightPollerEventTypeCardMismatch: {
        /* The poller only compares tag type, so this means the wrong sticker. */
        const MfUltralightData* present = nfc_poller_get_data(app->poller);
        app->detected_type = present->type;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NfcBusinessCardCustomEventWriteMismatch);
        command = NfcCommandStop;
        break;
    }

    case MfUltralightPollerEventTypeCardLocked:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NfcBusinessCardCustomEventWriteLocked);
        command = NfcCommandStop;
        break;

    case MfUltralightPollerEventTypeWriteSuccess:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NfcBusinessCardCustomEventWriteSuccess);
        command = NfcCommandStop;
        break;

    case MfUltralightPollerEventTypeWriteFail:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NfcBusinessCardCustomEventWriteFail);
        command = NfcCommandStop;
        break;

    case MfUltralightPollerEventTypeReadFailed:
        /* Tag moved out of range before the write started; keep looking. */
        command = NfcCommandReset;
        break;

    default:
        break;
    }

    return command;
}

void nfc_business_card_scene_write_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Widget* widget = app->widget;
    FuriString* text = app->tmp;

    widget_reset(widget);

    CardTagInfo info;
    card_tag_info(&app->my_card, &info);

    if(!info.fits || !card_tag_build(&app->my_card, app->nfc_device)) {
        nfc_business_card_show_card_error(app, &info, "Nothing to write");
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
        return;
    }

    app->write_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);

    widget_add_string_element(widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Write to tag");
    furi_string_printf(text, "Hold a blank %s against", info.tag_name);
    widget_add_string_element(
        widget, 64, 27, AlignCenter, AlignTop, FontSecondary, furi_string_get_cstr(text));
    widget_add_string_element(
        widget, 64, 38, AlignCenter, AlignTop, FontSecondary, "the back of the Flipper");
    furi_string_printf(text, "%u bytes", (unsigned)info.payload_size);
    widget_add_string_element(
        widget, 64, 53, AlignCenter, AlignTop, FontSecondary, furi_string_get_cstr(text));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);

    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->poller, nfc_business_card_scene_write_poller_callback, app);

    nfc_business_card_blink_scan(app);
}

bool nfc_business_card_scene_write_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type != SceneManagerEventTypeCustom) return false;

    const char* title = NULL;
    FuriString* text = app->tmp;
    bool success = false;

    switch(event.event) {
    case NfcBusinessCardCustomEventWriteSuccess: {
        CardTagInfo info;
        card_tag_info(&app->my_card, &info);
        title = "Tag written";
        furi_string_printf(
            text,
            "%u bytes on the %s.\nTap a phone to it to check.",
            (unsigned)info.payload_size,
            info.tag_name);
        success = true;
        consumed = true;
        break;
    }

    case NfcBusinessCardCustomEventWriteMismatch: {
        CardTagInfo info;
        card_tag_info(&app->my_card, &info);
        title = "Wrong tag type";
        furi_string_printf(
            text,
            "Your card is built for %s,\nbut that tag is a %s.",
            info.tag_name,
            card_tag_type_name(app->detected_type));
        consumed = true;
        break;
    }

    case NfcBusinessCardCustomEventWriteLocked:
        title = "Tag is locked";
        furi_string_set_str(
            text, "That tag is write-protected\nor password-locked.\nTry a blank one.");
        consumed = true;
        break;

    case NfcBusinessCardCustomEventWriteFail:
        title = "Write failed";
        furi_string_set_str(text, "Hold the tag still against\nthe Flipper and try again.");
        consumed = true;
        break;

    default:
        break;
    }

    if(consumed) {
        nfc_business_card_blink_stop(app);
        notification_message(app->notifications, success ? &sequence_success : &sequence_error);

        widget_reset(app->widget);
        widget_add_string_element(app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, title);
        widget_add_text_box_element(
            app->widget, 0, 26, 128, 38, AlignCenter, AlignTop, furi_string_get_cstr(text), false);
    }

    return consumed;
}

void nfc_business_card_scene_write_on_exit(void* context) {
    NfcBusinessCard* app = context;

    if(app->poller != NULL) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    app->write_data = NULL;
    nfc_business_card_blink_stop(app);
    widget_reset(app->widget);
}
