/**
 * @file nfc_business_card_i.h
 * @brief Shared application state.
 */
#pragma once

#include <furi.h>

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#include "card/business_card.h"
#include "card/card_tag.h"
#include "scenes/nfc_business_card_scene.h"

#define NFC_BC_BASE_FOLDER     "/ext/apps_data"
#define NFC_BC_APP_FOLDER      NFC_BC_BASE_FOLDER "/nfc_business_card"
#define NFC_BC_CONTACTS_FOLDER NFC_BC_APP_FOLDER "/contacts"
#define NFC_BC_MY_CARD_PATH    NFC_BC_APP_FOLDER "/my_card.txt"
#define NFC_BC_VCF_EXTENSION   ".vcf"

typedef enum {
    NfcBusinessCardViewSubmenu,
    NfcBusinessCardViewTextInput,
    NfcBusinessCardViewWidget,
    NfcBusinessCardViewPopup,
} NfcBusinessCardViewId;

/**
 * Custom events. Numbering starts above the range used for submenu item
 * indices, which are sent through the same channel.
 */
typedef enum {
    NfcBusinessCardCustomEventScanSuccess = 100,
    NfcBusinessCardCustomEventScanFailed,
    NfcBusinessCardCustomEventScanNoNdef,
    NfcBusinessCardCustomEventListenerActivated,
    NfcBusinessCardCustomEventSave,
    NfcBusinessCardCustomEventDelete,
    NfcBusinessCardCustomEventBack,
    NfcBusinessCardCustomEventWriteSuccess,
    NfcBusinessCardCustomEventWriteFail,
    NfcBusinessCardCustomEventWriteMismatch,
    NfcBusinessCardCustomEventWriteLocked,
} NfcBusinessCardCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Storage* storage;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    Popup* popup;

    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* listener;
    NfcPoller* poller;
    MfUltralightData* scanned_data;

    /** Tag image handed to the write poller; owned by ::nfc_device. */
    const MfUltralightData* write_data;
    /** Type of the tag actually presented, for the mismatch message. */
    MfUltralightType detected_type;

    /** The user's own contact, persisted to ::NFC_BC_MY_CARD_PATH. */
    BusinessCard my_card;
    /** Whatever was last scanned, or loaded from the saved list. */
    BusinessCard scanned_card;

    char text_buffer[BUSINESS_CARD_FIELD_SIZE];
    BusinessCardField edit_field;

    FuriString* file_path;
    FuriString* tmp;
} NfcBusinessCard;

/** Persist the user's card; shows nothing on failure beyond the return value. */
bool nfc_business_card_save_my_card(NfcBusinessCard* app);

void nfc_business_card_blink_emulate(NfcBusinessCard* app);
void nfc_business_card_blink_scan(NfcBusinessCard* app);
void nfc_business_card_blink_stop(NfcBusinessCard* app);

/** Lay a contact out as one line per populated field, for the detail views. */
void nfc_business_card_format_contact(const BusinessCard* card, FuriString* out);

/**
 * Shared screen for a card that yields no NDEF records, or is too long to fit.
 * Adds elements to the widget; the caller resets and switches to it.
 */
void nfc_business_card_show_card_error(
    NfcBusinessCard* app,
    const CardTagInfo* info,
    const char* title);

/** Build a .vcf path under the contacts folder that does not overwrite an existing file. */
void nfc_business_card_contact_path(
    NfcBusinessCard* app,
    const BusinessCard* card,
    FuriString* out);
