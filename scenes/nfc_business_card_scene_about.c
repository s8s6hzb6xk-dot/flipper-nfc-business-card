#include "../nfc_business_card_i.h"

#define NFC_BC_ABOUT_TEXT                 \
    "NFC Business Card\n"                 \
    "\n"                                  \
    "Share: the Flipper emulates an\n"    \
    "NTAG215/216 holding your card as\n"  \
    "an NDEF message. Hold a phone\n"     \
    "against the back of the Flipper.\n"  \
    "\n"                                  \
    "iPhone acts on URL records only.\n"  \
    "It ignores vCard records when it\n"  \
    "reads a tag, so leave Share as on\n" \
    "URL and point Website at a page\n"   \
    "holding your details.\n"             \
    "\n"                                  \
    "Android imports a vCard record\n"    \
    "natively, no page needed.\n"         \
    "\n"                                  \
    "Write to Tag: puts the same\n"       \
    "message on a real blank NTAG215\n"   \
    "sticker. More reliable than\n"       \
    "emulation. Only pages 4+ are\n"      \
    "written, so the tag keeps its\n"     \
    "own UID.\n"                          \
    "\n"                                  \
    "Scan: reads NTAG21x and MIFARE\n"    \
    "Ultralight tags and pulls out a\n"   \
    "vCard or URL record.\n"              \
    "\n"                                  \
    "Saved contacts become .vcf files\n"  \
    "in /ext/apps_data/\n"                \
    "nfc_business_card/contacts, so\n"    \
    "you can copy them off the SD card\n" \
    "and import them anywhere.\n"         \
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
