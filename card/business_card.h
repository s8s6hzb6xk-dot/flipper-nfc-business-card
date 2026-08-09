/**
 * @file business_card.h
 * @brief The contact record itself: fields, vCard 3.0 serialisation, and storage.
 */
#pragma once

#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Per-field capacity, including the NUL terminator. */
#define BUSINESS_CARD_FIELD_SIZE (64)

typedef enum {
    BusinessCardFieldFirstName,
    BusinessCardFieldLastName,
    BusinessCardFieldTitle,
    BusinessCardFieldCompany,
    BusinessCardFieldPhone,
    BusinessCardFieldEmail,
    BusinessCardFieldUrl,
    BusinessCardFieldNote,

    BusinessCardFieldCount,
} BusinessCardField;

/**
 * Which NDEF records to put on the emulated tag.
 *
 * A vCard record is what Android hands to the contacts importer. iOS does not
 * offer to import a vCard from a tag, but it will surface a URL record, so a
 * link is the portable option.
 */
typedef enum {
    BusinessCardShareVcard,
    BusinessCardShareUrl,
    BusinessCardShareVcardAndUrl,

    BusinessCardShareCount,
} BusinessCardShareMode;

typedef struct {
    char field[BusinessCardFieldCount][BUSINESS_CARD_FIELD_SIZE];
    BusinessCardShareMode share_mode;
} BusinessCard;

/** Clear every field and restore the default share mode. */
void business_card_reset(BusinessCard* card);

/** True when no field holds anything. */
bool business_card_is_empty(const BusinessCard* card);

/** Menu label for a field, e.g. "First name". */
const char* business_card_field_label(BusinessCardField field);

/** Menu label for a share mode, e.g. "vCard + URL". */
const char* business_card_share_mode_label(BusinessCardShareMode mode);

const char* business_card_get(const BusinessCard* card, BusinessCardField field);
void business_card_set(BusinessCard* card, BusinessCardField field, const char* value);

/** "First Last", falling back to company, then to "Unnamed". */
void business_card_display_name(const BusinessCard* card, FuriString* out);

/** A filename-safe version of the display name, without extension. */
void business_card_file_stem(const BusinessCard* card, FuriString* out);

/** Render as a vCard 3.0 document with CRLF line endings. */
void business_card_to_vcard(const BusinessCard* card, FuriString* out);

/** Populate from a vCard document. Returns true if BEGIN:VCARD was seen. */
bool business_card_from_vcard(BusinessCard* card, const char* vcard, size_t size);

/** Persist the card as a Flipper-format file. */
bool business_card_save(const BusinessCard* card, Storage* storage, const char* path);

/** Restore a card saved by ::business_card_save. */
bool business_card_load(BusinessCard* card, Storage* storage, const char* path);

/** Write a raw .vcf file, importable on a desktop or phone straight off the SD card. */
bool business_card_save_vcf(const BusinessCard* card, Storage* storage, const char* path);

/** Read a .vcf file written by ::business_card_save_vcf. */
bool business_card_load_vcf(BusinessCard* card, Storage* storage, const char* path);

#ifdef __cplusplus
}
#endif
