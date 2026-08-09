"""
Shared handling for the Flipper business card file format.

Mirrors card/business_card.c: the same storage keys, the same vCard escaping,
the same rule that empty fields are omitted rather than written blank. Keeping
it in one place means the writer, the page generator and the firmware can't
drift apart.
"""

from __future__ import annotations

from pathlib import Path

FILETYPE = "Flipper NFC Business Card"
VERSION = 1

#: Field capacity in the firmware, minus the NUL terminator.
FIELD_MAX = 95

#: Ordered to match BusinessCardField in card/business_card.h.
FIELDS = ("first", "last", "title", "company", "phone", "email", "url", "note")

#: Field name -> storage key, matching business_card_keys[] in business_card.c.
STORAGE_KEYS = {
    "first": "FirstName",
    "last": "LastName",
    "title": "Title",
    "company": "Company",
    "phone": "Phone",
    "email": "Email",
    "url": "Url",
    "note": "Note",
}

KEYS_TO_FIELDS = {key: field for field, key in STORAGE_KEYS.items()}

#: Matching BusinessCardShareMode in card/business_card.h.
SHARE_MODES = {"vcard": 0, "url": 1, "both": 2}
SHARE_MODE_NAMES = {0: "vCard", 1: "URL", 2: "vCard + URL"}


def read_card(path: Path) -> dict[str, str]:
    """Parse a my_card.txt written by the app (or by write_card)."""
    fields: dict[str, str] = {}

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        key, sep, value = line.partition(":")
        if not sep:
            continue
        key = key.strip()
        value = value.strip()

        if key == "Filetype" and value != FILETYPE:
            raise ValueError(f"{path}: not a {FILETYPE} file (got {value!r})")
        if key in KEYS_TO_FIELDS:
            fields[KEYS_TO_FIELDS[key]] = value
        elif key == "ShareMode":
            fields["share_mode"] = value

    return fields


def write_card(path: Path, fields: dict[str, str], share_mode: int = 1) -> None:
    """
    Write a my_card.txt the firmware will load.

    Empty fields are omitted, matching business_card_save(): the reader treats a
    missing key as an empty field, and flipper_format cannot round-trip a blank
    value.
    """
    lines = [f"Filetype: {FILETYPE}", f"Version: {VERSION}"]

    for field in FIELDS:
        value = fields.get(field, "").strip()
        if value:
            lines.append(f"{STORAGE_KEYS[field]}: {value}")

    lines.append(f"ShareMode: {share_mode}")

    # The firmware writes LF; keep newline="" so Windows doesn't turn them into CRLF.
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="")


def check_lengths(fields: dict[str, str]) -> list[str]:
    """Return warnings for values the firmware would truncate."""
    warnings = []
    for field in FIELDS:
        value = fields.get(field, "")
        if len(value) > FIELD_MAX:
            warnings.append(
                f"{field}: {len(value)} chars, the Flipper stores {FIELD_MAX} "
                f"— will be cut to {value[:FIELD_MAX]!r}"
            )
    return warnings


def display_name(fields: dict[str, str]) -> str:
    """"First Last", falling back to company, then "Unnamed"."""
    name = " ".join(part for part in (fields.get("first"), fields.get("last")) if part)
    return name or fields.get("company") or "Unnamed"


def vcard_escape(value: str) -> str:
    """Escape per RFC 2426, matching vcard_append_escaped() in business_card.c."""
    out = []
    for char in value:
        if char in ("\\", ";", ","):
            out.append("\\" + char)
        elif char == "\n":
            out.append("\\n")
        elif char != "\r":
            out.append(char)
    return "".join(out)


def build_vcard(fields: dict[str, str]) -> str:
    """Render as vCard 3.0 with CRLF line endings, matching business_card_to_vcard()."""
    lines = [
        "BEGIN:VCARD",
        "VERSION:3.0",
        f"N:{vcard_escape(fields.get('last', ''))};"
        f"{vcard_escape(fields.get('first', ''))};;;",
        f"FN:{vcard_escape(display_name(fields))}",
    ]

    for prop, field in (
        ("ORG", "company"),
        ("TITLE", "title"),
        ("TEL;TYPE=CELL", "phone"),
        ("EMAIL;TYPE=INTERNET", "email"),
        ("URL", "url"),
        ("NOTE", "note"),
    ):
        value = fields.get(field)
        if value:
            lines.append(f"{prop}:{vcard_escape(value)}")

    lines.append("END:VCARD")
    return "\r\n".join(lines) + "\r\n"
