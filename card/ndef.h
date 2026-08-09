/**
 * @file ndef.h
 * @brief Minimal NDEF encoder/decoder for NFC Forum Type 2 tags.
 *
 * Type 2 tags store their NDEF message inside a TLV block in user memory:
 *
 *     0x03 <length> <ndef message> 0xFE
 *
 * where <length> is one byte when the message is shorter than 255 bytes, and
 * 0xFF followed by a big-endian uint16 otherwise. A vCard with a few filled-in
 * fields regularly crosses 255 bytes, so both encodings are handled here.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Type Name Format values used by this app (NFCForum-TS-NDEF, section 3.2.6). */
#define NDEF_TNF_WELL_KNOWN (0x01)
#define NDEF_TNF_MIME       (0x02)

/** MIME type Android maps to the contacts importer. */
#define NDEF_MIME_VCARD "text/vcard"

/** A single record to serialise. */
typedef struct {
    uint8_t tnf;
    const char* type; /**< NUL-terminated record type, e.g. "text/vcard" or "U". */
    const uint8_t* payload;
    size_t payload_size;
} NdefRecord;

/** Kinds of payload the decoder recognises. */
typedef enum {
    NdefPayloadKindUnknown,
    NdefPayloadKindVcard,
    NdefPayloadKindUri,
    NdefPayloadKindText,
} NdefPayloadKind;

/**
 * @brief Bytes an NDEF TLV holding these records will occupy.
 *
 * Includes the TLV tag, length field and terminator, so the result can be
 * compared directly against a tag's usable user memory.
 */
size_t ndef_tlv_size(const NdefRecord* records, size_t record_count);

/**
 * @brief Serialise records into a complete NDEF-message TLV.
 *
 * @return bytes written, or 0 if the message does not fit in @p out_size.
 */
size_t
    ndef_tlv_build(uint8_t* out, size_t out_size, const NdefRecord* records, size_t record_count);

/**
 * @brief Build a prefix-compressed URI record payload.
 *
 * The first payload byte is a well-known prefix index (e.g. 0x04 for
 * "https://"), which keeps common URLs a few bytes shorter on the tag.
 *
 * @return payload size, or 0 if it does not fit in @p out_size.
 */
size_t ndef_uri_payload(uint8_t* out, size_t out_size, const char* uri);

/**
 * @brief Called once per decoded record.
 *
 * For URI records the payload is already expanded to a full NUL-terminated
 * string; for the other kinds it points into the caller's buffer and is NOT
 * NUL-terminated, so @p payload_size must be respected.
 */
typedef void (*NdefRecordDecodedCallback)(
    NdefPayloadKind kind,
    const uint8_t* payload,
    size_t payload_size,
    void* context);

/**
 * @brief Walk a tag's user memory and decode every NDEF record found.
 *
 * @return true if a well-formed NDEF message TLV was present.
 */
bool ndef_tlv_parse(
    const uint8_t* data,
    size_t data_size,
    NdefRecordDecodedCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
