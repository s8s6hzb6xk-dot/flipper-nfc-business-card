#include "business_card.h"

#include <flipper_format/flipper_format.h>
#include <string.h>

#define BUSINESS_CARD_FILE_TYPE    "Flipper NFC Business Card"
#define BUSINESS_CARD_FILE_VERSION (1)

/** Upper bound on a .vcf we are willing to read into RAM. */
#define BUSINESS_CARD_VCF_MAX (4096)

static const char* const business_card_labels[BusinessCardFieldCount] = {
    [BusinessCardFieldFirstName] = "First name",
    [BusinessCardFieldLastName] = "Last name",
    [BusinessCardFieldTitle] = "Job title",
    [BusinessCardFieldCompany] = "Company",
    [BusinessCardFieldPhone] = "Phone",
    [BusinessCardFieldEmail] = "Email",
    [BusinessCardFieldUrl] = "Website",
    [BusinessCardFieldNote] = "Note",
};

/** Storage keys, kept separate from labels so renaming a label cannot break saved files. */
static const char* const business_card_keys[BusinessCardFieldCount] = {
    [BusinessCardFieldFirstName] = "FirstName",
    [BusinessCardFieldLastName] = "LastName",
    [BusinessCardFieldTitle] = "Title",
    [BusinessCardFieldCompany] = "Company",
    [BusinessCardFieldPhone] = "Phone",
    [BusinessCardFieldEmail] = "Email",
    [BusinessCardFieldUrl] = "Url",
    [BusinessCardFieldNote] = "Note",
};

static const char* const business_card_share_labels[BusinessCardShareCount] = {
    [BusinessCardShareVcard] = "vCard",
    [BusinessCardShareUrl] = "URL",
    [BusinessCardShareVcardAndUrl] = "vCard + URL",
};

void business_card_reset(BusinessCard* card) {
    furi_check(card);
    memset(card, 0, sizeof(BusinessCard));
    card->share_mode = BusinessCardShareVcard;
}

bool business_card_is_empty(const BusinessCard* card) {
    furi_check(card);
    for(size_t i = 0; i < BusinessCardFieldCount; i++) {
        if(card->field[i][0] != '\0') return false;
    }
    return true;
}

const char* business_card_field_label(BusinessCardField field) {
    furi_check(field < BusinessCardFieldCount);
    return business_card_labels[field];
}

const char* business_card_share_mode_label(BusinessCardShareMode mode) {
    furi_check(mode < BusinessCardShareCount);
    return business_card_share_labels[mode];
}

const char* business_card_get(const BusinessCard* card, BusinessCardField field) {
    furi_check(card);
    furi_check(field < BusinessCardFieldCount);
    return card->field[field];
}

void business_card_set(BusinessCard* card, BusinessCardField field, const char* value) {
    furi_check(card);
    furi_check(field < BusinessCardFieldCount);

    char* dest = card->field[field];
    if(value == NULL) {
        dest[0] = '\0';
        return;
    }

    size_t i = 0;
    while(value[i] != '\0' && i < BUSINESS_CARD_FIELD_SIZE - 1) {
        dest[i] = value[i];
        i++;
    }
    dest[i] = '\0';
}

void business_card_display_name(const BusinessCard* card, FuriString* out) {
    furi_check(card);
    furi_check(out);

    furi_string_reset(out);
    if(card->field[BusinessCardFieldFirstName][0] != '\0') {
        furi_string_cat_str(out, card->field[BusinessCardFieldFirstName]);
    }
    if(card->field[BusinessCardFieldLastName][0] != '\0') {
        if(furi_string_size(out) > 0) furi_string_push_back(out, ' ');
        furi_string_cat_str(out, card->field[BusinessCardFieldLastName]);
    }
    if(furi_string_size(out) == 0) {
        furi_string_set_str(out, card->field[BusinessCardFieldCompany]);
    }
    if(furi_string_size(out) == 0) {
        furi_string_set_str(out, "Unnamed");
    }
}

void business_card_file_stem(const BusinessCard* card, FuriString* out) {
    business_card_display_name(card, out);

    /* Keep the name usable as a FAT filename. */
    for(size_t i = 0; i < furi_string_size(out); i++) {
        char c = furi_string_get_char(out, i);
        bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == ' ' || c == '.';
        if(!safe) furi_string_set_char(out, i, '_');
    }
}

/* ------------------------------------------------------------------ vCard out */

static void vcard_append_escaped(FuriString* out, const char* value) {
    for(const char* p = value; *p != '\0'; p++) {
        char c = *p;
        if(c == '\\' || c == ';' || c == ',') {
            furi_string_push_back(out, '\\');
            furi_string_push_back(out, c);
        } else if(c == '\n') {
            furi_string_cat_str(out, "\\n");
        } else if(c != '\r') {
            furi_string_push_back(out, c);
        }
    }
}

static void vcard_append_property(FuriString* out, const char* property, const char* value) {
    if(value[0] == '\0') return;
    furi_string_cat_str(out, property);
    furi_string_push_back(out, ':');
    vcard_append_escaped(out, value);
    furi_string_cat_str(out, "\r\n");
}

void business_card_to_vcard(const BusinessCard* card, FuriString* out) {
    furi_check(card);
    furi_check(out);

    furi_string_set_str(out, "BEGIN:VCARD\r\nVERSION:3.0\r\n");

    /* N and FN are both mandatory in vCard 3.0. N is Family;Given;Middle;Prefix;Suffix. */
    furi_string_cat_str(out, "N:");
    vcard_append_escaped(out, card->field[BusinessCardFieldLastName]);
    furi_string_push_back(out, ';');
    vcard_append_escaped(out, card->field[BusinessCardFieldFirstName]);
    furi_string_cat_str(out, ";;;\r\n");

    FuriString* display = furi_string_alloc();
    business_card_display_name(card, display);
    furi_string_cat_str(out, "FN:");
    vcard_append_escaped(out, furi_string_get_cstr(display));
    furi_string_cat_str(out, "\r\n");
    furi_string_free(display);

    vcard_append_property(out, "ORG", card->field[BusinessCardFieldCompany]);
    vcard_append_property(out, "TITLE", card->field[BusinessCardFieldTitle]);
    vcard_append_property(out, "TEL;TYPE=CELL", card->field[BusinessCardFieldPhone]);
    vcard_append_property(out, "EMAIL;TYPE=INTERNET", card->field[BusinessCardFieldEmail]);
    vcard_append_property(out, "URL", card->field[BusinessCardFieldUrl]);
    vcard_append_property(out, "NOTE", card->field[BusinessCardFieldNote]);

    furi_string_cat_str(out, "END:VCARD\r\n");
}

/* ------------------------------------------------------------------- vCard in */

static bool vcard_name_is(const char* line, size_t name_len, const char* expected) {
    if(strlen(expected) != name_len) return false;
    for(size_t i = 0; i < name_len; i++) {
        char a = line[i];
        char b = expected[i];
        if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
        if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
        if(a != b) return false;
    }
    return true;
}

static void vcard_unescape(char* dest, size_t dest_size, const char* src, size_t src_len) {
    size_t out = 0;
    for(size_t i = 0; i < src_len && out + 1 < dest_size; i++) {
        char c = src[i];
        if(c == '\\' && i + 1 < src_len) {
            i++;
            c = src[i];
            /* Our fields are single-line, so an escaped newline becomes a space. */
            if(c == 'n' || c == 'N') c = ' ';
        }
        dest[out++] = c;
    }
    dest[out] = '\0';
}

static void
    vcard_set_field(BusinessCard* card, BusinessCardField field, const char* src, size_t src_len) {
    if(card->field[field][0] != '\0') return; /* first occurrence wins */
    vcard_unescape(card->field[field], BUSINESS_CARD_FIELD_SIZE, src, src_len);
}

/** Locate structured component @p index of a value, honouring backslash escapes. */
static bool vcard_get_component(const char* value, size_t index, const char** start, size_t* len) {
    size_t current = 0;
    const char* segment = value;

    for(const char* p = value;; p++) {
        if(*p == '\\' && *(p + 1) != '\0') {
            p++;
            continue;
        }
        if(*p == ';' || *p == '\0') {
            if(current == index) {
                *start = segment;
                *len = (size_t)(p - segment);
                return true;
            }
            if(*p == '\0') return false;
            current++;
            segment = p + 1;
        }
    }
}

static void vcard_process_line(BusinessCard* card, const char* line, bool* begin_seen) {
    const char* colon = strchr(line, ':');
    if(colon == NULL) return;

    /* Property name runs to the first ';' (parameters) or ':' (value). */
    size_t name_len = 0;
    while(line[name_len] != ':' && line[name_len] != ';' && line[name_len] != '\0') {
        name_len++;
    }

    const char* value = colon + 1;
    size_t value_len = strlen(value);

    if(vcard_name_is(line, name_len, "BEGIN")) {
        if(vcard_name_is(value, value_len, "VCARD")) *begin_seen = true;

    } else if(vcard_name_is(line, name_len, "N")) {
        const char* part;
        size_t part_len;
        if(vcard_get_component(value, 0, &part, &part_len) && part_len > 0) {
            vcard_set_field(card, BusinessCardFieldLastName, part, part_len);
        }
        if(vcard_get_component(value, 1, &part, &part_len) && part_len > 0) {
            vcard_set_field(card, BusinessCardFieldFirstName, part, part_len);
        }

    } else if(vcard_name_is(line, name_len, "FN")) {
        /* Only used when the card carried no structured N. */
        if(card->field[BusinessCardFieldFirstName][0] == '\0' &&
           card->field[BusinessCardFieldLastName][0] == '\0') {
            const char* space = strchr(value, ' ');
            if(space != NULL) {
                vcard_set_field(card, BusinessCardFieldFirstName, value, (size_t)(space - value));
                vcard_set_field(card, BusinessCardFieldLastName, space + 1, strlen(space + 1));
            } else {
                vcard_set_field(card, BusinessCardFieldFirstName, value, value_len);
            }
        }

    } else if(vcard_name_is(line, name_len, "ORG")) {
        const char* part;
        size_t part_len;
        if(vcard_get_component(value, 0, &part, &part_len)) {
            vcard_set_field(card, BusinessCardFieldCompany, part, part_len);
        }

    } else if(vcard_name_is(line, name_len, "TITLE")) {
        vcard_set_field(card, BusinessCardFieldTitle, value, value_len);
    } else if(vcard_name_is(line, name_len, "TEL")) {
        vcard_set_field(card, BusinessCardFieldPhone, value, value_len);
    } else if(vcard_name_is(line, name_len, "EMAIL")) {
        vcard_set_field(card, BusinessCardFieldEmail, value, value_len);
    } else if(vcard_name_is(line, name_len, "URL")) {
        vcard_set_field(card, BusinessCardFieldUrl, value, value_len);
    } else if(vcard_name_is(line, name_len, "NOTE")) {
        vcard_set_field(card, BusinessCardFieldNote, value, value_len);
    }
}

bool business_card_from_vcard(BusinessCard* card, const char* vcard, size_t size) {
    furi_check(card);
    if(vcard == NULL || size == 0) return false;

    business_card_reset(card);

    bool begin_seen = false;
    FuriString* line = furi_string_alloc();
    size_t pos = 0;

    while(pos < size) {
        while(pos < size && vcard[pos] != '\r' && vcard[pos] != '\n') {
            furi_string_push_back(line, vcard[pos]);
            pos++;
        }
        if(pos < size && vcard[pos] == '\r') pos++;
        if(pos < size && vcard[pos] == '\n') pos++;

        /* A leading space or tab folds this physical line into the previous one. */
        if(pos < size && (vcard[pos] == ' ' || vcard[pos] == '\t')) {
            pos++;
            continue;
        }

        vcard_process_line(card, furi_string_get_cstr(line), &begin_seen);
        furi_string_reset(line);
    }

    if(furi_string_size(line) > 0) {
        vcard_process_line(card, furi_string_get_cstr(line), &begin_seen);
    }

    furi_string_free(line);
    return begin_seen;
}

/* ------------------------------------------------------------------- storage */

bool business_card_save(const BusinessCard* card, Storage* storage, const char* path) {
    furi_check(card);
    furi_check(storage);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool success = false;

    do {
        if(!flipper_format_file_open_always(ff, path)) break;
        if(!flipper_format_write_header_cstr(
               ff, BUSINESS_CARD_FILE_TYPE, BUSINESS_CARD_FILE_VERSION))
            break;

        bool fields_written = true;
        for(size_t i = 0; i < BusinessCardFieldCount; i++) {
            if(card->field[i][0] == '\0') continue; /* empty values are simply omitted */
            if(!flipper_format_write_string_cstr(ff, business_card_keys[i], card->field[i])) {
                fields_written = false;
                break;
            }
        }
        if(!fields_written) break;

        uint32_t mode = (uint32_t)card->share_mode;
        if(!flipper_format_write_uint32(ff, "ShareMode", &mode, 1)) break;

        success = true;
    } while(false);

    flipper_format_free(ff);
    return success;
}

bool business_card_load(BusinessCard* card, Storage* storage, const char* path) {
    furi_check(card);
    furi_check(storage);

    business_card_reset(card);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool success = false;

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version)) break;
        if(furi_string_cmp_str(tmp, BUSINESS_CARD_FILE_TYPE) != 0) break;
        if(version != BUSINESS_CARD_FILE_VERSION) break;

        /* Missing keys are expected — the writer omits empty fields. */
        for(size_t i = 0; i < BusinessCardFieldCount; i++) {
            flipper_format_rewind(ff);
            if(flipper_format_read_string(ff, business_card_keys[i], tmp)) {
                business_card_set(card, i, furi_string_get_cstr(tmp));
            }
        }

        flipper_format_rewind(ff);
        uint32_t mode = 0;
        if(flipper_format_read_uint32(ff, "ShareMode", &mode, 1) &&
           mode < BusinessCardShareCount) {
            card->share_mode = (BusinessCardShareMode)mode;
        }

        success = true;
    } while(false);

    furi_string_free(tmp);
    flipper_format_free(ff);
    return success;
}

bool business_card_save_vcf(const BusinessCard* card, Storage* storage, const char* path) {
    furi_check(card);
    furi_check(storage);

    FuriString* vcard = furi_string_alloc();
    business_card_to_vcard(card, vcard);

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t size = furi_string_size(vcard);
        success = storage_file_write(file, furi_string_get_cstr(vcard), size) == size;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(vcard);
    return success;
}

bool business_card_load_vcf(BusinessCard* card, Storage* storage, const char* path) {
    furi_check(card);
    furi_check(storage);

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t file_size = storage_file_size(file);
        if(file_size > 0 && file_size <= BUSINESS_CARD_VCF_MAX) {
            size_t size = (size_t)file_size;
            char* buffer = malloc(size + 1);
            if(storage_file_read(file, buffer, size) == size) {
                buffer[size] = '\0';
                success = business_card_from_vcard(card, buffer, size);
            }
            free(buffer);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    return success;
}
