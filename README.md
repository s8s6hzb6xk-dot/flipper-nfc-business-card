# NFC Business Card

A Flipper Zero app that turns the device into your business card — and into a scanner for
everyone else's.

**Share** puts your details on the air by emulating an NTAG215/216 carrying an NDEF message.
Hold a phone against the back of the Flipper — no app needed on the phone.

**Write to Tag** puts that same message on a real blank NTAG sticker. A written sticker reads
perfectly on every phone with none of the emulation quirks, so it's the reliable option when it
matters.

**Scan** reads NTAG21x and MIFARE Ultralight tags, pulls the vCard or URL out of the NDEF
message, and saves it as a `.vcf` file you can import anywhere.

```
┌──────────────────────────┐     ┌──────────────────────────┐
│ NFC Business Card        │     │      Ada Lovelace        │
│                          │     │                          │
│ > Share My Card          │     │   Hold a phone against   │
│   Scan a Card            │     │  the back of the Flipper │
│   Write to Tag           │     │                          │
│   Edit My Card           │     │    NTAG215  187/504 B    │
│   Saved Contacts         │     │                          │
└──────────────────────────┘     └──────────────────────────┘
```

## Status

Builds clean against the current SDK (API 87.1, target 7) and the NDEF codec is covered by
unit tests. It has not yet been exercised against real hardware — if you try it on a Flipper,
issues and reports are welcome.

Two hardware caveats worth knowing before you rely on this at an event. Flipper tag emulation
is imperfect: it can't detect the reader's exact carrier frequency the way a real tag does, so
some phones pick it up instantly and others need a nudge. And there's a [known firmware
bug][emu-bug] where reading an EMV or DESFire card leaves the Flipper undetectable while
emulating until you read an NTAG or reboot. If a phone won't see the Flipper, reboot it first
before assuming this app is at fault.

If emulation turns out to be flaky with the phones you actually meet, use **Write to Tag** and a
real NTAG215 sticker. A written sticker reads perfectly on every phone, and there's no Flipper
involved at the moment of handoff.

[emu-bug]: https://github.com/flipperdevices/flipperzero-firmware/issues/2298

## Install

Grab `nfc_business_card.fap` from the [Actions artifacts][actions] (or build it yourself,
below) and drop it in `SD/apps/NFC/` on the Flipper. It shows up under **Apps → NFC**.

[actions]: ../../actions

## Using it

**Edit My Card** — fill in the fields you want to hand out: name, job title, company, phone,
email, website, note. Everything is optional; blank fields are simply left off the vCard. The
card is saved as you go.

Typing all that on the Flipper's keypad is no fun. [Build the card on your computer](#filling-in-your-card-without-typing)
instead and copy one file across.

At the bottom of that menu, **Share as** picks which NDEF records go on the tag:

| Mode | What lands on the tag | Works on |
| --- | --- | --- |
| `URL` *(default)* | One URI record built from your website field | iPhone and Android |
| `vCard` | One `text/vcard` MIME record | Android only — **iPhone ignores it** |
| `vCard + URL` | Both records in one message | Android imports natively, iPhone follows the URL |

**Share My Card** builds the tag and starts emulating. The footer shows which tag type was
picked and how much of it you're using. The LED blinks magenta while the field is live.

**Write to Tag** puts the same message on a real blank sticker. Hold the tag against the back of
the Flipper until it reports success. Buy **NTAG215** — it's the most widely sold sticker (504
usable bytes, the Amiibo standard) and it's what the app targets unless your contact is long
enough to need an NTAG216. The screen tells you which type it wants before you present anything,
and if you present the wrong one it names what it found rather than failing vaguely.

Only pages 4 and up are written, so the tag keeps its own UID and its factory capability
container. Nothing here can brick a sticker — the worst case is a failed write you can retry.
Write-protected or password-locked tags are reported as locked rather than hammered at.

**Scan a Card** polls for a tag and decodes whatever NDEF it finds. You get the parsed contact
on screen and a **Save** button.

**Saved Contacts** lists everything you've saved, with a **Delete** button on each entry.

## Phone compatibility

**Android** reads the vCard record directly and offers to add the contact. Nothing else needed.

**iPhone is the constraint, and it's a hard one.** When iOS reads a tag in the background it
looks for a *URI record* and acts on that. It does not understand `text/vcard` MIME records and
will not offer to import a contact from one — [Apple's own docs][apple-bg] say background reading
inspects the message for a URI record and ignores unsupported types. This is why every commercial
NFC business card (Popl, Linq, Mobilo) puts a *link* on the tag rather than a contact record.

So on iPhone the chain is: tag holds a URL → Safari opens your contact page → you tap **Add to
Contacts**. That's why `URL` is the default share mode.

One more iOS wrinkle worth knowing: since iOS 13 a `.vcf` link opens in a *preview* rather than
jumping straight into Contacts — the person has to tap the share icon and pick Contacts. The
generated page says so out loud, and also renders your phone and email as `tel:`/`mailto:` links
so there's a one-tap path either way.

[apple-bg]: https://developer.apple.com/documentation/corenfc/adding-support-for-background-tag-reading

## Filling in your card without typing

```bash
python tools/make_card.py --first Ada --last Lovelace --title "Lead Engineer" --company "Analytical Engines" --phone +15551234567 --email ada@example.com --url https://example.github.io/card/
```

That writes `my_card.txt` in the same format the app itself saves. Copy it to the Flipper at:

```
/ext/apps_data/nfc_business_card/my_card.txt
```

with qFlipper's file manager, or by putting the SD card in a reader.

**Close the app before you copy.** It reads the card once at startup, and it saves on leaving
the editor — so a running app can write its own empty card back over the file you just placed.
Copy first, then launch.

To change one field later without retyping the rest:

```bash
python tools/make_card.py --from my_card.txt --phone +15559876543
```

`my_card.txt` is gitignored, since it holds your details and this repo is public.

## Your contact page

`URL` mode needs somewhere to point. The repo ships a generator that reads the **same
`my_card.txt` the app writes**, so your tag and your page can't drift apart:

```bash
python tools/make_contact_page.py --card my_card.txt
```

It writes `docs/index.html` (a self-contained page — no external fonts, scripts or trackers,
light and dark aware) plus `docs/contact.vcf`. Or skip the card file and pass fields directly:

```bash
python tools/make_contact_page.py --first Luke --last Guttenberg --email you@example.com --phone +15551234567
```

Whatever you put on that page is public once you publish it — a phone number on GitHub Pages is
permanently indexable. Put on it only what you'd hand to a stranger, because that is exactly what
it is.

To publish it free on GitHub Pages:

1. Commit the generated `docs/` folder and push.
2. **Settings → Pages → Source: Deploy from a branch**, branch `main`, folder `/docs`.
3. Wait for the green tick, then open `https://<username>.github.io/<repo>/` to check it.
4. Put that URL in the app's **Website** field, and leave **Share as** on `URL`.

Copy `my_card.txt` off the Flipper at `/ext/apps_data/nfc_business_card/my_card.txt` — via
qFlipper's file manager, or by pulling the SD card.

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
tools/
├── flipper_card.py          the card file format, shared by both scripts
├── make_card.py             builds my_card.txt so you don't type it on the Flipper
└── make_contact_page.py     builds the hosted contact page from your card
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
- Contact fields hold 95 characters each.
- The vCard parser handles the properties this app writes — `N`, `FN`, `ORG`, `TITLE`, `TEL`,
  `EMAIL`, `URL`, `NOTE` — including line folding and escapes. Quoted-printable encoding, used
  by some older vCard 2.1 exporters, is not decoded.

## License

MIT — see [LICENSE](LICENSE).
