# NFC Business Card

A Flipper Zero app that turns the device into your business card — and into a scanner for
everyone else's.

**Share** puts your contact details on the air as an NDEF vCard by emulating an NTAG215/216.
Hold a phone against the back of the Flipper and it offers to save the contact, with no app
on the phone's side.

**Scan** reads NTAG21x and MIFARE Ultralight tags, pulls the vCard or URL out of the NDEF
message, and saves it to the SD card as a `.vcf` file you can import anywhere.

```
┌──────────────────────────┐     ┌──────────────────────────┐
│ NFC Business Card        │     │      Ada Lovelace        │
│                          │     │                          │
│ > Share My Card          │     │   Hold a phone against   │
│   Scan a Card            │     │  the back of the Flipper │
│   Edit My Card           │     │                          │
│   Saved Contacts         │     │    NTAG215  187/504 B    │
│   About                  │     │                          │
└──────────────────────────┘     └──────────────────────────┘
```

## Status

Builds clean against the current SDK (API 87.1, target 7) and the NDEF codec is covered by
unit tests. It has not yet been exercised against real hardware — if you try it on a Flipper,
issues and reports are welcome.

## Install

Grab `nfc_business_card.fap` from the [Actions artifacts][actions] (or build it yourself,
below) and drop it in `SD/apps/NFC/` on the Flipper. It shows up under **Apps → NFC**.

[actions]: ../../actions

## Using it

**Edit My Card** — fill in the fields you want to hand out: name, job title, company, phone,
email, website, note. Everything is optional; blank fields are simply left off the vCard. The
card is saved as you go.

At the bottom of that menu, **Share as** picks which NDEF records go on the tag:

| Mode | What lands on the tag | Best for |
| --- | --- | --- |
| `vCard` | One `text/vcard` MIME record | Android — hands straight to the contacts importer |
| `URL` | One URI record built from your website field | iOS, and any reader that only understands links |
| `vCard + URL` | Both records in one message | Mixed crowds |

**Share My Card** builds the tag and starts emulating. The footer shows which tag type was
picked and how much of it you're using. The LED blinks magenta while the field is live.

**Scan a Card** polls for a tag and decodes whatever NDEF it finds. You get the parsed contact
on screen and a **Save** button.

**Saved Contacts** lists everything you've saved, with a **Delete** button on each entry.

## Phone compatibility

Android reads the vCard record directly and offers to add the contact.

iOS is fussier. iOS 14+ reads NDEF tags in the background, but it surfaces **URL records**, not
vCard MIME records — it won't offer to import a contact from a tag. If you need to hand your
details to iPhone users, set **Share as** to `URL` (or `vCard + URL`) and point the website
field at a page or a hosted `.vcf`.

## How it works

The Flipper emulates an NFC Forum Type 2 tag. `nfc_data_generator_fill_data()` produces a
factory-valid NTAG215 or NTAG216 — correct UID with its BCC, lock bytes, and a capability
container of `E1 10 3E 00` marking the tag NDEF-formatted — and the app overwrites the user
memory (page 4 onward) with its own NDEF message TLV:

```
0x03  <length>  <ndef message>  0xFE
```

The length field is one byte below 255 and `0xFF` plus a big-endian `uint16` at or above it.
That distinction matters here: a vCard with a title, company, phone, email and URL clears 255
bytes easily, and getting it wrong produces a tag that reads as empty. The same threshold
applies inside the record header, where payloads under 256 bytes use the Short Record form.

The tag type is chosen by size — NTAG215 (504 usable bytes) unless the contact needs NTAG216's
888. If it fits nowhere, the share screen says so instead of silently truncating.

Scanning runs the same machinery backwards: an `MfUltralightPoller` reads the tag, and the
decoder walks the TLV area from page 4, skipping lock-control and memory-control TLVs, and
stops before the dynamic lock and configuration pages so their contents are never mistaken for
data. Password-protected pages are skipped rather than attacked — the scanner never guesses
credentials.

## On the SD card

```
/ext/apps_data/nfc_business_card/
├── my_card.txt              your own contact, Flipper-format
└── contacts/
    ├── Ada Lovelace.vcf     scanned contacts, standard vCard 3.0
    └── Grace Hopper.vcf
```

Saved contacts are plain `.vcf` files on purpose: copy them off the SD card and they import
into any phone, mail client or address book without this app in the loop.

## Repo layout

```
application.fam              app manifest
nfc_business_card.c          entry point, app lifecycle
nfc_business_card_i.h        shared app state
card/
├── business_card.[ch]       contact fields, vCard 3.0 in and out, storage
├── ndef.[ch]                NDEF encoder/decoder (no SDK dependency)
└── card_tag.[ch]            contact <-> emulatable NTAG
scenes/                      one file per screen
test/test_ndef.c             host-side codec tests
```

## Building

```bash
python -m pip install --upgrade ufbt
ufbt
```

The `.fap` lands in `dist/`. To build and launch it on a connected Flipper:

```bash
ufbt launch
```

## Tests

`card/ndef.c` depends on nothing but the C standard library, so the byte-level format work is
testable on a normal machine:

```bash
gcc -std=gnu11 -Wall -Wextra -Werror -fsanitize=address,undefined -o test_ndef test/test_ndef.c card/ndef.c && ./test_ndef
```

The suite covers short and long TLV length forms, Short Record versus 4-byte payload lengths,
MB/ME flags across multi-record messages, URI prefix compression, a full encode/decode round
trip, and a set of malformed tags that must not read past the end of the buffer. CI runs it
under ASan and UBSan on every push.

## Limitations

- Emulates and reads Type 2 tags (NTAG21x, MIFARE Ultralight). NDEF stored on MIFARE Classic
  is not supported.
- Contact fields hold 63 characters each.
- The vCard parser handles the properties this app writes — `N`, `FN`, `ORG`, `TITLE`, `TEL`,
  `EMAIL`, `URL`, `NOTE` — including line folding and escapes. Quoted-printable encoding, used
  by some older vCard 2.1 exporters, is not decoded.

## License

MIT — see [LICENSE](LICENSE).
