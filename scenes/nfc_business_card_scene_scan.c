#include "../nfc_business_card_i.h"

static NfcCommand
    nfc_business_card_scene_scan_poller_callback(NfcGenericEvent event, void* context) {
    NfcBusinessCard* app = context;
    NfcCommand command = NfcCommandContinue;

    furi_assert(event.protocol == NfcProtocolMfUltralight);
    MfUltralightPollerEvent* mfu_event = event.event_data;

    switch(mfu_event->type) {
    case MfUltralightPollerEventTypeRequestMode:
        mfu_event->data->poller_mode = MfUltralightPollerModeRead;
        break;

    case MfUltralightPollerEventTypeAuthRequest:
        /* Read whatever the tag exposes publicly; never guess passwords. */
        mfu_event->data->auth_context.skip_auth = true;
        break;

    case MfUltralightPollerEventTypeReadSuccess:
        mf_ultralight_copy(app->scanned_data, nfc_poller_get_data(app->poller));
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NfcBusinessCardCustomEventScanSuccess);
        command = NfcCommandStop;
        break;

    case MfUltralightPollerEventTypeReadFailed:
        /* Tag pulled away mid-read: go back to looking for one. */
        command = NfcCommandReset;
        break;

    default:
        break;
    }

    return command;
}

static void nfc_business_card_scene_scan_show_searching(NfcBusinessCard* app) {
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Scanning for a card");
    widget_add_string_element(
        widget, 64, 28, AlignCenter, AlignTop, FontSecondary, "Hold the Flipper over");
    widget_add_string_element(
        widget, 64, 39, AlignCenter, AlignTop, FontSecondary, "an NFC tag or another");
    widget_add_string_element(
        widget, 64, 50, AlignCenter, AlignTop, FontSecondary, "Flipper sharing a card");
}

void nfc_business_card_scene_scan_on_enter(void* context) {
    NfcBusinessCard* app = context;

    nfc_business_card_scene_scan_show_searching(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);

    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->poller, nfc_business_card_scene_scan_poller_callback, app);

    nfc_business_card_blink_scan(app);
}

bool nfc_business_card_scene_scan_on_event(void* context, SceneManagerEvent event) {
    NfcBusinessCard* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcBusinessCardCustomEventScanSuccess) {
        nfc_business_card_blink_stop(app);

        if(card_tag_extract(app->scanned_data, &app->scanned_card)) {
            scene_manager_next_scene(app->scene_manager, NfcBusinessCardSceneScanResult);
        } else {
            /* A tag was read, it just wasn't carrying a contact. */
            Widget* widget = app->widget;
            widget_reset(widget);
            widget_add_string_element(
                widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "No contact on tag");
            widget_add_text_box_element(
                widget,
                0,
                26,
                128,
                38,
                AlignCenter,
                AlignTop,
                "The tag was read but holds no\nvCard or URL record.",
                false);
        }

        consumed = true;
    }

    return consumed;
}

void nfc_business_card_scene_scan_on_exit(void* context) {
    NfcBusinessCard* app = context;

    if(app->poller != NULL) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    nfc_business_card_blink_stop(app);
    widget_reset(app->widget);
}
