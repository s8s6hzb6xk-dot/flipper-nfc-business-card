#include "card_tag.h"
#include "ndef.h"

#include <nfc/helpers/nfc_data_generator.h>
#include <string.h>

/**
 * User-memory extents per tag type, from the NXP datasheets. Page 0-3 hold the
 * UID, lock bytes and capability container; the pages after the user area are
 * the dynamic lock and configuration pages.
 */
typedef struct {
    MfUltralightType type;
    NfcDataGeneratorType generator;
    const char* name;
    uint8_t first_page;
    uint16_t page_count;
} CardTagProfile;

static const CardTagProfile card_tag_profiles[] = {
    {MfUltralightTypeNTAG215, NfcDataGeneratorTypeNTAG215, "NTAG215", 4, 126}, /* 504 bytes */
    {MfUltralightTypeNTAG216, NfcDataGeneratorTypeNTAG216, "NTAG216", 4, 222}, /* 888 bytes */
};

#define CARD_TAG_PROFILE_COUNT (sizeof(card_tag_profiles) / sizeof(card_tag_profiles[0]))

static size_t card_tag_profile_capacity(const CardTagProfile* profile) {
    return (size_t)profile->page_count * MF_ULTRALIGHT_PAGE_SIZE;
}

/**
 * Assemble the card's NDEF TLV.
 *
 * With @p out NULL this only measures, so the UI can report the size before
 * anything is written.
 */
static size_t card_tag_assemble(const BusinessCard* card, uint8_t* out, size_t out_size) {
    FuriString* vcard = furi_string_alloc();
    uint8_t uri_payload[BUSINESS_CARD_FIELD_SIZE + 1];
    NdefRecord records[2];
    size_t count = 0;

    bool want_vcard = card->share_mode != BusinessCardShareUrl;
    bool want_url = (card->share_mode != BusinessCardShareVcard) &&
                    (business_card_get(card, BusinessCardFieldUrl)[0] != '\0');

    if(want_vcard) {
        business_card_to_vcard(card, vcard);
        records[count].tnf = NDEF_TNF_MIME;
        records[count].type = NDEF_MIME_VCARD;
        records[count].payload = (const uint8_t*)furi_string_get_cstr(vcard);
        records[count].payload_size = furi_string_size(vcard);
        count++;
    }

    if(want_url) {
        size_t uri_size = ndef_uri_payload(
            uri_payload, sizeof(uri_payload), business_card_get(card, BusinessCardFieldUrl));
        if(uri_size > 0) {
            records[count].tnf = NDEF_TNF_WELL_KNOWN;
            records[count].type = "U";
            records[count].payload = uri_payload;
            records[count].payload_size = uri_size;
            count++;
        }
    }

    size_t size = 0;
    if(count > 0) {
        size = (out == NULL) ? ndef_tlv_size(records, count) :
                               ndef_tlv_build(out, out_size, records, count);
    }

    furi_string_free(vcard);
    return size;
}

/** Smallest profile the payload fits on, or the largest one if it fits nowhere. */
static const CardTagProfile* card_tag_pick_profile(size_t payload_size) {
    for(size_t i = 0; i < CARD_TAG_PROFILE_COUNT; i++) {
        if(payload_size <= card_tag_profile_capacity(&card_tag_profiles[i])) {
            return &card_tag_profiles[i];
        }
    }
    return &card_tag_profiles[CARD_TAG_PROFILE_COUNT - 1];
}

void card_tag_info(const BusinessCard* card, CardTagInfo* info) {
    furi_check(card);
    furi_check(info);

    size_t payload_size = card_tag_assemble(card, NULL, 0);
    const CardTagProfile* profile = card_tag_pick_profile(payload_size);

    info->payload_size = payload_size;
    info->capacity = card_tag_profile_capacity(profile);
    info->tag_name = profile->name;
    info->fits = (payload_size > 0) && (payload_size <= info->capacity);
}

bool card_tag_build(const BusinessCard* card, NfcDevice* device) {
    furi_check(card);
    furi_check(device);

    size_t payload_size = card_tag_assemble(card, NULL, 0);
    if(payload_size == 0) return false;

    const CardTagProfile* profile = card_tag_pick_profile(payload_size);
    size_t capacity = card_tag_profile_capacity(profile);
    if(payload_size > capacity) return false;

    uint8_t* buffer = malloc(capacity);
    memset(buffer, 0, capacity);

    bool success = false;
    if(card_tag_assemble(card, buffer, capacity) == payload_size) {
        /* Start from a factory-valid tag so the UID BCC, lock bytes and
         * capability container are all correct for this tag type. */
        nfc_data_generator_fill_data(profile->generator, device);

        MfUltralightData* data = mf_ultralight_alloc();
        mf_ultralight_copy(data, nfc_device_get_data(device, NfcProtocolMfUltralight));

        for(uint16_t i = 0; i < profile->page_count; i++) {
            uint16_t page = profile->first_page + i;
            if(page >= data->pages_total) break;

            size_t offset = (size_t)i * MF_ULTRALIGHT_PAGE_SIZE;
            memset(data->page[page].data, 0, MF_ULTRALIGHT_PAGE_SIZE);

            if(offset < payload_size) {
                size_t chunk = payload_size - offset;
                if(chunk > MF_ULTRALIGHT_PAGE_SIZE) chunk = MF_ULTRALIGHT_PAGE_SIZE;
                memcpy(data->page[page].data, &buffer[offset], chunk);
            }
        }

        nfc_device_set_data(device, NfcProtocolMfUltralight, data);
        mf_ultralight_free(data);
        success = true;
    }

    free(buffer);
    return success;
}

/* ------------------------------------------------------------------ scanning */

typedef struct {
    FuriString* vcard;
    FuriString* url;
    bool has_vcard;
    bool has_url;
} CardTagParse;

static void card_tag_append(FuriString* out, const uint8_t* payload, size_t payload_size) {
    for(size_t i = 0; i < payload_size; i++) {
        furi_string_push_back(out, (char)payload[i]);
    }
}

static void card_tag_record_callback(
    NdefPayloadKind kind,
    const uint8_t* payload,
    size_t payload_size,
    void* context) {
    CardTagParse* parse = context;

    if(kind == NdefPayloadKindVcard && !parse->has_vcard) {
        card_tag_append(parse->vcard, payload, payload_size);
        parse->has_vcard = true;
    } else if(kind == NdefPayloadKindUri && !parse->has_url) {
        card_tag_append(parse->url, payload, payload_size);
        parse->has_url = true;
    }
}

bool card_tag_extract(const MfUltralightData* data, BusinessCard* card) {
    furi_check(data);
    furi_check(card);

    /* Stop before the dynamic lock and configuration pages so their contents
     * are never mistaken for TLV data. */
    uint16_t end_page = mf_ultralight_get_config_page_num(data->type);
    end_page = (end_page > 0) ? (uint16_t)(end_page - 1) : data->pages_read;
    if(end_page > data->pages_read) end_page = data->pages_read;

    const uint16_t first_page = 4;
    if(end_page <= first_page) return false;

    const uint8_t* user_memory = (const uint8_t*)&data->page[first_page];
    size_t user_size = (size_t)(end_page - first_page) * MF_ULTRALIGHT_PAGE_SIZE;

    CardTagParse parse = {
        .vcard = furi_string_alloc(),
        .url = furi_string_alloc(),
        .has_vcard = false,
        .has_url = false,
    };

    ndef_tlv_parse(user_memory, user_size, card_tag_record_callback, &parse);

    bool success = false;
    if(parse.has_vcard) {
        success = business_card_from_vcard(
            card, furi_string_get_cstr(parse.vcard), furi_string_size(parse.vcard));
    }

    if(!success && parse.has_url) {
        /* A URL-only tag still carries something worth keeping. */
        business_card_reset(card);
        success = true;
    }

    if(success && parse.has_url && business_card_get(card, BusinessCardFieldUrl)[0] == '\0') {
        business_card_set(card, BusinessCardFieldUrl, furi_string_get_cstr(parse.url));
    }

    furi_string_free(parse.vcard);
    furi_string_free(parse.url);
    return success;
}
