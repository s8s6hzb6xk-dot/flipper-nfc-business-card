#include "../nfc_business_card_i.h"

#define NFC_BC_ABOUT_TEXT                 \
    "NFC Business Card\n"                 \
    "\n"                                  \
    "Share: the Flipper emulates an\n"    \
    "NTAG215/216 holding your contact\n"  \
    "as an NDEF vCard. Hold a phone\n"    \
    "against the back of the Flipper.\n"  \
    "\n"                                  \
    "Scan: reads NTAG21x and MIFARE\n"    \
    "Ultralight tags and pulls out a\n"   \
    "vCard or URL record.\n"              \
    "\n"                                  \
    "Saved contacts are written as\n"     \
    ".vcf files to\n"                     \
    "/ext/apps_data/nfc_business_card/\n" \
    "contacts, so you can copy them\n"    \
    "off the SD card and import them\n"   \
    "anywhere.\n"                         \
    "\n"                                  \
    "Share as:\n"                         \
    "  vCard - Android imports it\n"      \
    "  URL - works on iOS too\n"          \
    "  vCard + URL - both records\n"      \
    "\n"                                  \
    "MIT licensed."

void nfc_business_card_scene_about_on_enter(void* context) {
    NfcBusinessCard* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, NFC_BC_ABOUT_TEXT);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcBusinessCardViewWidget);
}

bool nfc_business_card_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_business_card_scene_about_on_exit(void* context) {
    NfcBusinessCard* app = context;
    widget_reset(app->widget);
}
