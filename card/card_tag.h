/**
 * @file card_tag.h
 * @brief Turns a contact into an emulatable NTAG, and a scanned NTAG back into a contact.
 */
#pragma once

#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include "business_card.h"

#ifdef __cplusplus
extern "C" {
#endif

/** What the card will look like once written to a tag. */
typedef struct {
    size_t payload_size; /**< Bytes the NDEF TLV occupies. */
    size_t capacity; /**< Usable NDEF bytes on the selected tag. */
    const char* tag_name; /**< e.g. "NTAG215". */
    bool fits; /**< False when the contact is too long for any supported tag. */
} CardTagInfo;

/** Describe the tag this card would be shared as, without building it. */
void card_tag_info(const BusinessCard* card, CardTagInfo* info);

/**
 * @brief Fill @p device with an NTAG carrying the card's NDEF message.
 *
 * Picks the smallest supported tag the payload fits on. The device is left
 * ready to hand straight to an NfcListener.
 *
 * @return false if the card produced no records, or is too long to fit.
 */
bool card_tag_build(const BusinessCard* card, NfcDevice* device);

/**
 * @brief Recover a contact from a tag that was just read.
 *
 * Accepts vCard MIME records; if the tag only carried a URI record, the URL
 * lands in the card's website field.
 *
 * @return false when the tag holds no NDEF message this app understands.
 */
bool card_tag_extract(const MfUltralightData* data, BusinessCard* card);

#ifdef __cplusplus
}
#endif
