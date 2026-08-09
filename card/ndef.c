#include "ndef.h"

#include <string.h>

/* TLV tags used in Type 2 tag user memory. */
#define TLV_NULL        (0x00)
#define TLV_LOCK_CTRL   (0x01)
#define TLV_MEM_CTRL    (0x02)
#define TLV_NDEF_MSG    (0x03)
#define TLV_PROPRIETARY (0xFD)
#define TLV_TERMINATOR  (0xFE)

/* NDEF record header flags. */
#define NDEF_FLAG_MB  (0x80) /**< Message Begin */
#define NDEF_FLAG_ME  (0x40) /**< Message End */
#define NDEF_FLAG_CF  (0x20) /**< Chunk Flag */
#define NDEF_FLAG_SR  (0x10) /**< Short Record */
#define NDEF_FLAG_IL  (0x08) /**< ID Length present */
#define NDEF_TNF_MASK (0x07)

/** Longest URI the decoder will expand before truncating. */
#define NDEF_URI_DECODE_MAX (256)

/** Well-known URI prefixes, indexed by the first byte of a URI record payload. */
static const char* const ndef_uri_prefix[] = {
    "",
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    "tel:",
    "mailto:",
    "ftp://anonymous:anonymous@",
    "ftp://ftp.",
    "ftps://",
    "sftp://",
    "smb://",
    "nfs://",
    "ftp://",
    "dav://",
    "news:",
    "telnet://",
    "imap:",
    "rtsp://",
    "urn:",
    "pop:",
    "sip:",
    "sips:",
    "tftp:",
    "btspp://",
    "btl2cap://",
    "btgoep://",
    "tcpobex://",
    "irdaobex://",
    "file://",
    "urn:epc:id:",
    "urn:epc:tag:",
    "urn:epc:pat:",
    "urn:epc:raw:",
    "urn:epc:",
    "urn:nfc:",
};

#define NDEF_URI_PREFIX_COUNT (sizeof(ndef_uri_prefix) / sizeof(ndef_uri_prefix[0]))

/** Case-insensitive comparison over a known length; the toolchain's strncasecmp is not guaranteed. */
static bool ndef_type_matches(const uint8_t* type, size_t type_size, const char* expected) {
    if(strlen(expected) != type_size) return false;
    for(size_t i = 0; i < type_size; i++) {
        char a = (char)type[i];
        char b = expected[i];
        if(a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if(b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if(a != b) return false;
    }
    return true;
}

static size_t ndef_record_size(const NdefRecord* record) {
    size_t type_len = (record->type != NULL) ? strlen(record->type) : 0;
    /* header + type length + payload length field + type + payload */
    size_t size = 1 + 1 + ((record->payload_size < 256) ? 1 : 4);
    return size + type_len + record->payload_size;
}

static size_t ndef_message_size(const NdefRecord* records, size_t record_count) {
    size_t size = 0;
    for(size_t i = 0; i < record_count; i++) {
        size += ndef_record_size(&records[i]);
    }
    return size;
}

size_t ndef_tlv_size(const NdefRecord* records, size_t record_count) {
    if(records == NULL || record_count == 0) return 0;
    size_t msg_size = ndef_message_size(records, record_count);
    size_t len_field = (msg_size < 0xFF) ? 1 : 3;
    /* tag + length + message + terminator */
    return 1 + len_field + msg_size + 1;
}

size_t
    ndef_tlv_build(uint8_t* out, size_t out_size, const NdefRecord* records, size_t record_count) {
    if(out == NULL || records == NULL || record_count == 0) return 0;

    size_t msg_size = ndef_message_size(records, record_count);
    if(msg_size > 0xFFFF) return 0;

    size_t total = ndef_tlv_size(records, record_count);
    if(total > out_size) return 0;

    size_t pos = 0;
    out[pos++] = TLV_NDEF_MSG;
    if(msg_size < 0xFF) {
        out[pos++] = (uint8_t)msg_size;
    } else {
        out[pos++] = 0xFF;
        out[pos++] = (uint8_t)(msg_size >> 8);
        out[pos++] = (uint8_t)(msg_size & 0xFF);
    }

    for(size_t i = 0; i < record_count; i++) {
        const NdefRecord* record = &records[i];
        size_t type_len = (record->type != NULL) ? strlen(record->type) : 0;
        bool short_record = record->payload_size < 256;

        uint8_t header = record->tnf & NDEF_TNF_MASK;
        if(i == 0) header |= NDEF_FLAG_MB;
        if(i == record_count - 1) header |= NDEF_FLAG_ME;
        if(short_record) header |= NDEF_FLAG_SR;

        out[pos++] = header;
        out[pos++] = (uint8_t)type_len;

        if(short_record) {
            out[pos++] = (uint8_t)record->payload_size;
        } else {
            out[pos++] = (uint8_t)(record->payload_size >> 24);
            out[pos++] = (uint8_t)(record->payload_size >> 16);
            out[pos++] = (uint8_t)(record->payload_size >> 8);
            out[pos++] = (uint8_t)(record->payload_size);
        }

        if(type_len > 0) {
            memcpy(&out[pos], record->type, type_len);
            pos += type_len;
        }
        if(record->payload_size > 0 && record->payload != NULL) {
            memcpy(&out[pos], record->payload, record->payload_size);
            pos += record->payload_size;
        }
    }

    out[pos++] = TLV_TERMINATOR;
    return pos;
}

size_t ndef_uri_payload(uint8_t* out, size_t out_size, const char* uri) {
    if(out == NULL || uri == NULL || out_size == 0) return 0;

    /* Pick the longest matching well-known prefix to keep the payload small. */
    uint8_t prefix_index = 0;
    size_t prefix_len = 0;
    for(size_t i = 1; i < NDEF_URI_PREFIX_COUNT; i++) {
        size_t len = strlen(ndef_uri_prefix[i]);
        if(len > prefix_len && strncmp(uri, ndef_uri_prefix[i], len) == 0) {
            prefix_index = (uint8_t)i;
            prefix_len = len;
        }
    }

    size_t rest = strlen(uri) - prefix_len;
    if(rest + 1 > out_size) return 0;

    out[0] = prefix_index;
    memcpy(&out[1], uri + prefix_len, rest);
    return rest + 1;
}

static void ndef_emit_uri(
    const uint8_t* payload,
    size_t payload_size,
    NdefRecordDecodedCallback callback,
    void* context) {
    char buffer[NDEF_URI_DECODE_MAX];

    uint8_t index = payload[0];
    const char* prefix = (index < NDEF_URI_PREFIX_COUNT) ? ndef_uri_prefix[index] : "";
    size_t prefix_len = strlen(prefix);
    size_t body_len = payload_size - 1;

    if(prefix_len >= sizeof(buffer)) prefix_len = sizeof(buffer) - 1;
    memcpy(buffer, prefix, prefix_len);

    size_t space = sizeof(buffer) - 1 - prefix_len;
    if(body_len > space) body_len = space;
    memcpy(buffer + prefix_len, payload + 1, body_len);
    buffer[prefix_len + body_len] = '\0';

    callback(NdefPayloadKindUri, (const uint8_t*)buffer, prefix_len + body_len, context);
}

static void ndef_emit_record(
    uint8_t tnf,
    const uint8_t* type,
    size_t type_size,
    const uint8_t* payload,
    size_t payload_size,
    NdefRecordDecodedCallback callback,
    void* context) {
    if(tnf == NDEF_TNF_MIME) {
        if(ndef_type_matches(type, type_size, "text/vcard") ||
           ndef_type_matches(type, type_size, "text/x-vcard") ||
           ndef_type_matches(type, type_size, "text/directory")) {
            callback(NdefPayloadKindVcard, payload, payload_size, context);
            return;
        }
    } else if(tnf == NDEF_TNF_WELL_KNOWN && type_size == 1) {
        if(type[0] == 'U' && payload_size >= 1) {
            ndef_emit_uri(payload, payload_size, callback, context);
            return;
        }
        if(type[0] == 'T' && payload_size >= 1) {
            /* Status byte: bits 0-5 hold the IANA language-code length. */
            size_t lang_len = payload[0] & 0x3F;
            if(1 + lang_len <= payload_size) {
                callback(
                    NdefPayloadKindText,
                    payload + 1 + lang_len,
                    payload_size - 1 - lang_len,
                    context);
                return;
            }
        }
    }

    callback(NdefPayloadKindUnknown, payload, payload_size, context);
}

static void ndef_message_parse(
    const uint8_t* msg,
    size_t msg_size,
    NdefRecordDecodedCallback callback,
    void* context) {
    size_t pos = 0;

    while(pos < msg_size) {
        uint8_t header = msg[pos++];
        if(pos >= msg_size) break;
        size_t type_len = msg[pos++];

        size_t payload_len;
        if(header & NDEF_FLAG_SR) {
            if(pos >= msg_size) break;
            payload_len = msg[pos++];
        } else {
            if(pos + 4 > msg_size) break;
            payload_len = ((size_t)msg[pos] << 24) | ((size_t)msg[pos + 1] << 16) |
                          ((size_t)msg[pos + 2] << 8) | (size_t)msg[pos + 3];
            pos += 4;
        }

        size_t id_len = 0;
        if(header & NDEF_FLAG_IL) {
            if(pos >= msg_size) break;
            id_len = msg[pos++];
        }

        if(pos + type_len > msg_size) break;
        const uint8_t* type = &msg[pos];
        pos += type_len;

        if(pos + id_len > msg_size) break;
        pos += id_len;

        if(pos + payload_len > msg_size) break;
        const uint8_t* payload = &msg[pos];
        pos += payload_len;

        ndef_emit_record(
            header & NDEF_TNF_MASK, type, type_len, payload, payload_len, callback, context);

        if(header & NDEF_FLAG_ME) break;
    }
}

bool ndef_tlv_parse(
    const uint8_t* data,
    size_t data_size,
    NdefRecordDecodedCallback callback,
    void* context) {
    if(data == NULL || data_size == 0 || callback == NULL) return false;

    size_t pos = 0;
    while(pos < data_size) {
        uint8_t tag = data[pos++];

        if(tag == TLV_TERMINATOR) break;
        if(tag == TLV_NULL) continue;

        if(pos >= data_size) break;
        size_t len = data[pos++];
        if(len == 0xFF) {
            if(pos + 2 > data_size) break;
            len = ((size_t)data[pos] << 8) | (size_t)data[pos + 1];
            pos += 2;
        }
        if(pos + len > data_size) break;

        if(tag == TLV_NDEF_MSG) {
            ndef_message_parse(&data[pos], len, callback, context);
            return true;
        }

        /* Lock control, memory control and proprietary TLVs are skipped. */
        pos += len;
    }

    return false;
}
