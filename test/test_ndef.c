/**
 * Host-side tests for the NDEF encoder/decoder.
 *
 * card/ndef.c deliberately depends on nothing but the C standard library, so
 * the byte-level format work — the part that decides whether a phone reads the
 * tag at all — can be checked without a Flipper or the SDK.
 *
 *   gcc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 *       -o test_ndef test/test_ndef.c card/ndef.c && ./test_ndef
 */
#include "../card/ndef.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond, ...)                                \
    do {                                                \
        checks++;                                       \
        if(!(cond)) {                                   \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while(0)

#define MAX_COLLECTED      8
#define MAX_COLLECTED_SIZE 1024

typedef struct {
    NdefPayloadKind kind[MAX_COLLECTED];
    char payload[MAX_COLLECTED][MAX_COLLECTED_SIZE];
    size_t size[MAX_COLLECTED];
    size_t count;
} Collected;

static void
    collect(NdefPayloadKind kind, const uint8_t* payload, size_t payload_size, void* context) {
    Collected* collected = context;
    if(collected->count >= MAX_COLLECTED) return;

    size_t copy = payload_size;
    if(copy > MAX_COLLECTED_SIZE - 1) copy = MAX_COLLECTED_SIZE - 1;

    memcpy(collected->payload[collected->count], payload, copy);
    collected->payload[collected->count][copy] = '\0';
    collected->size[collected->count] = payload_size;
    collected->kind[collected->count] = kind;
    collected->count++;
}

/* --------------------------------------------------------------------- encode */

static void test_encode_short_vcard(void) {
    const char* vcard = "BEGIN:VCARD\r\nEND:VCARD\r\n"; /* 24 bytes */
    NdefRecord record = {
        .tnf = NDEF_TNF_MIME,
        .type = NDEF_MIME_VCARD,
        .payload = (const uint8_t*)vcard,
        .payload_size = strlen(vcard),
    };

    uint8_t out[128];
    size_t size = ndef_tlv_build(out, sizeof(out), &record, 1);

    /* 1 header + 1 type len + 1 payload len + 10 type + 24 payload = 37 */
    CHECK(ndef_tlv_size(&record, 1) == 40, "tlv_size was %zu, want 40", ndef_tlv_size(&record, 1));
    CHECK(size == 40, "built %zu bytes, want 40", size);

    CHECK(out[0] == 0x03, "TLV tag was 0x%02X, want 0x03", out[0]);
    CHECK(out[1] == 37, "TLV length was %u, want 37", out[1]);
    /* MB | ME | SR | TNF=MIME */
    CHECK(out[2] == 0xD2, "record header was 0x%02X, want 0xD2", out[2]);
    CHECK(out[3] == 10, "type length was %u, want 10", out[3]);
    CHECK(out[4] == 24, "payload length was %u, want 24", out[4]);
    CHECK(memcmp(&out[5], "text/vcard", 10) == 0, "type field mismatch");
    CHECK(memcmp(&out[15], vcard, 24) == 0, "payload mismatch");
    CHECK(out[39] == 0xFE, "terminator was 0x%02X, want 0xFE", out[39]);
}

/** A realistic vCard crosses both the 255-byte TLV and 256-byte record thresholds. */
static void test_encode_long_payload(void) {
    char payload[300];
    memset(payload, 'A', sizeof(payload));

    NdefRecord record = {
        .tnf = NDEF_TNF_MIME,
        .type = NDEF_MIME_VCARD,
        .payload = (const uint8_t*)payload,
        .payload_size = sizeof(payload),
    };

    uint8_t out[512];
    size_t size = ndef_tlv_build(out, sizeof(out), &record, 1);

    /* record = 1 + 1 + 4 + 10 + 300 = 316; TLV = 1 + 3 + 316 + 1 = 321 */
    CHECK(size == 321, "built %zu bytes, want 321", size);
    CHECK(out[0] == 0x03, "TLV tag was 0x%02X", out[0]);
    CHECK(out[1] == 0xFF, "expected 3-byte length marker, got 0x%02X", out[1]);
    CHECK(
        out[2] == 0x01 && out[3] == 0x3C, "TLV length was 0x%02X%02X, want 0x013C", out[2], out[3]);
    /* MB | ME | TNF=MIME, no SR because the payload exceeds 255 bytes */
    CHECK(out[4] == 0xC2, "record header was 0x%02X, want 0xC2", out[4]);
    CHECK(out[5] == 10, "type length was %u, want 10", out[5]);
    CHECK(
        out[6] == 0 && out[7] == 0 && out[8] == 0x01 && out[9] == 0x2C,
        "payload length was %02X%02X%02X%02X, want 0000012C",
        out[6],
        out[7],
        out[8],
        out[9]);
    CHECK(out[320] == 0xFE, "terminator was 0x%02X", out[320]);
}

static void test_encode_two_records(void) {
    const char* vcard = "BEGIN:VCARD\r\nEND:VCARD\r\n";
    uint8_t uri[64];
    size_t uri_size = ndef_uri_payload(uri, sizeof(uri), "https://example.com");

    NdefRecord records[2] = {
        {
            .tnf = NDEF_TNF_MIME,
            .type = NDEF_MIME_VCARD,
            .payload = (const uint8_t*)vcard,
            .payload_size = strlen(vcard),
        },
        {
            .tnf = NDEF_TNF_WELL_KNOWN,
            .type = "U",
            .payload = uri,
            .payload_size = uri_size,
        },
    };

    uint8_t out[256];
    size_t size = ndef_tlv_build(out, sizeof(out), records, 2);
    CHECK(size > 0, "two-record build failed");

    /* First record: MB set, ME clear. */
    CHECK((out[2] & 0x80) != 0, "first record missing MB");
    CHECK((out[2] & 0x40) == 0, "first record must not set ME");

    /* Second record starts right after the first (37 bytes in). */
    uint8_t second = out[2 + 37];
    CHECK((second & 0x80) == 0, "second record must not set MB");
    CHECK((second & 0x40) != 0, "second record missing ME");
    CHECK((second & 0x07) == NDEF_TNF_WELL_KNOWN, "second record TNF was %u", second & 0x07);
}

static void test_encode_rejects_overflow(void) {
    char payload[600];
    memset(payload, 'B', sizeof(payload));

    NdefRecord record = {
        .tnf = NDEF_TNF_MIME,
        .type = NDEF_MIME_VCARD,
        .payload = (const uint8_t*)payload,
        .payload_size = sizeof(payload),
    };

    uint8_t out[64];
    CHECK(ndef_tlv_build(out, sizeof(out), &record, 1) == 0, "build should refuse to overflow");
    CHECK(ndef_tlv_build(out, sizeof(out), NULL, 0) == 0, "build should reject an empty message");
}

/* ------------------------------------------------------------------------ URI */

static void test_uri_prefix_compression(void) {
    uint8_t out[64];

    size_t size = ndef_uri_payload(out, sizeof(out), "https://example.com");
    CHECK(size == 12, "https payload was %zu bytes, want 12", size);
    CHECK(out[0] == 0x04, "https prefix index was 0x%02X, want 0x04", out[0]);
    CHECK(memcmp(&out[1], "example.com", 11) == 0, "https body mismatch");

    size = ndef_uri_payload(out, sizeof(out), "http://www.acme.io");
    CHECK(out[0] == 0x01, "http://www. prefix index was 0x%02X, want 0x01", out[0]);
    CHECK(memcmp(&out[1], "acme.io", 7) == 0, "http://www. body mismatch");

    /* Longest match wins: https://www. (0x02) beats https:// (0x04). */
    size = ndef_uri_payload(out, sizeof(out), "https://www.acme.io");
    CHECK(out[0] == 0x02, "https://www. prefix index was 0x%02X, want 0x02", out[0]);

    size = ndef_uri_payload(out, sizeof(out), "mailto:a@b.com");
    CHECK(out[0] == 0x06, "mailto prefix index was 0x%02X, want 0x06", out[0]);

    /* No known prefix falls back to 0x00 and the untouched URI. */
    size = ndef_uri_payload(out, sizeof(out), "xyz://host");
    CHECK(out[0] == 0x00, "unknown scheme prefix was 0x%02X, want 0x00", out[0]);
    CHECK(size == 11, "unknown scheme payload was %zu bytes, want 11", size);

    uint8_t tiny[4];
    CHECK(
        ndef_uri_payload(tiny, sizeof(tiny), "https://example.com") == 0,
        "uri payload should refuse to overflow");
}

/* --------------------------------------------------------------------- decode */

static void test_decode_round_trip(void) {
    const char* vcard = "BEGIN:VCARD\r\nFN:Ada Lovelace\r\nEND:VCARD\r\n";
    uint8_t uri[64];
    size_t uri_size = ndef_uri_payload(uri, sizeof(uri), "https://example.com/ada");

    NdefRecord records[2] = {
        {
            .tnf = NDEF_TNF_MIME,
            .type = NDEF_MIME_VCARD,
            .payload = (const uint8_t*)vcard,
            .payload_size = strlen(vcard),
        },
        {
            .tnf = NDEF_TNF_WELL_KNOWN,
            .type = "U",
            .payload = uri,
            .payload_size = uri_size,
        },
    };

    /* Sit the message inside a page-sized buffer, the way it lands on a tag. */
    uint8_t tag[504];
    memset(tag, 0, sizeof(tag));
    size_t size = ndef_tlv_build(tag, sizeof(tag), records, 2);
    CHECK(size > 0, "round-trip build failed");

    Collected collected = {0};
    CHECK(ndef_tlv_parse(tag, sizeof(tag), collect, &collected), "parse found no NDEF TLV");
    CHECK(collected.count == 2, "decoded %zu records, want 2", collected.count);

    if(collected.count == 2) {
        CHECK(
            collected.kind[0] == NdefPayloadKindVcard, "record 0 kind was %d", collected.kind[0]);
        CHECK(collected.size[0] == strlen(vcard), "record 0 size was %zu", collected.size[0]);
        CHECK(strcmp(collected.payload[0], vcard) == 0, "record 0 payload mismatch");

        CHECK(collected.kind[1] == NdefPayloadKindUri, "record 1 kind was %d", collected.kind[1]);
        CHECK(
            strcmp(collected.payload[1], "https://example.com/ada") == 0,
            "URI expanded to \"%s\"",
            collected.payload[1]);
    }
}

/** Real tags often carry a lock-control TLV ahead of the NDEF one. */
static void test_decode_skips_leading_tlvs(void) {
    uint8_t tag[64] = {
        0x01,
        0x03,
        0xA0,
        0x10,
        0x44, /* lock control TLV */
        0x03,
        0x0B, /* NDEF TLV, 11 bytes */
        0xD1,
        0x01,
        0x07,
        'U',
        0x04,
        'a',
        '.',
        'c',
        'o',
        'm', /* URI record */
        0xFE,
    };

    Collected collected = {0};
    CHECK(ndef_tlv_parse(tag, sizeof(tag), collect, &collected), "parse missed the NDEF TLV");
    CHECK(collected.count == 1, "decoded %zu records, want 1", collected.count);
    if(collected.count == 1) {
        CHECK(collected.kind[0] == NdefPayloadKindUri, "kind was %d", collected.kind[0]);
        CHECK(
            strcmp(collected.payload[0], "https://a.com") == 0,
            "URI expanded to \"%s\"",
            collected.payload[0]);
    }
}

static void test_decode_text_record(void) {
    uint8_t tag[32] = {
        0x03,
        0x0C, /* NDEF TLV, 12 bytes */
        0xD1,
        0x01,
        0x08,
        'T',
        0x02,
        'e',
        'n',
        'H',
        'e',
        'l',
        'l',
        'o',
        0xFE,
    };

    Collected collected = {0};
    CHECK(ndef_tlv_parse(tag, sizeof(tag), collect, &collected), "parse missed the NDEF TLV");
    CHECK(collected.count == 1, "decoded %zu records, want 1", collected.count);
    if(collected.count == 1) {
        CHECK(collected.kind[0] == NdefPayloadKindText, "kind was %d", collected.kind[0]);
        /* Status byte and the "en" language code are stripped. */
        CHECK(strcmp(collected.payload[0], "Hello") == 0, "text was \"%s\"", collected.payload[0]);
    }
}

/** A blank tag is all zeroes: TLV 0x00 is NULL padding and must not loop forever. */
static void test_decode_blank_tag(void) {
    uint8_t tag[128];
    memset(tag, 0, sizeof(tag));

    Collected collected = {0};
    CHECK(
        !ndef_tlv_parse(tag, sizeof(tag), collect, &collected), "blank tag reported an NDEF TLV");
    CHECK(collected.count == 0, "blank tag produced %zu records", collected.count);
}

/**
 * Malformed input must stay inside the buffer. Under ASAN/UBSAN an overrun
 * here fails the run rather than returning a wrong answer.
 */
static void test_decode_malformed(void) {
    Collected collected = {0};

    /* Length runs past the end of the buffer. */
    uint8_t truncated[8] = {0x03, 0x40, 0xD1, 0x01, 0x03, 'U', 0x04, 'a'};
    ndef_tlv_parse(truncated, sizeof(truncated), collect, &collected);

    /* Record claims more payload than the message holds. */
    uint8_t bad_record[10] = {0x03, 0x06, 0xD1, 0x01, 0xFF, 'U', 0x04, 'a', 'b', 0xFE};
    ndef_tlv_parse(bad_record, sizeof(bad_record), collect, &collected);

    /* Header promises an ID field that was never written. */
    uint8_t bad_id[7] = {0x03, 0x03, 0xD9, 0x01, 0x01, 'U', 0xFE};
    ndef_tlv_parse(bad_id, sizeof(bad_id), collect, &collected);

    /* Three-byte length marker with nothing after it. */
    uint8_t bad_len[2] = {0x03, 0xFF};
    ndef_tlv_parse(bad_len, sizeof(bad_len), collect, &collected);

    /* NDEF TLV with no records and no terminator. */
    uint8_t empty[2] = {0x03, 0x00};
    ndef_tlv_parse(empty, sizeof(empty), collect, &collected);

    CHECK(!ndef_tlv_parse(NULL, 0, collect, &collected), "NULL input should report no TLV");
    CHECK(1, "malformed inputs handled without overrunning");
}

int main(void) {
    test_encode_short_vcard();
    test_encode_long_payload();
    test_encode_two_records();
    test_encode_rejects_overflow();
    test_uri_prefix_compression();
    test_decode_round_trip();
    test_decode_skips_leading_tlvs();
    test_decode_text_record();
    test_decode_blank_tag();
    test_decode_malformed();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
