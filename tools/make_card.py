#!/usr/bin/env python3
"""
Build a my_card.txt on your computer instead of typing it on the Flipper.

Copy the result to the Flipper at:

    /ext/apps_data/nfc_business_card/my_card.txt

using qFlipper's file manager, or by putting the SD card in a reader. Restart
the app afterwards — it reads the card once at startup.

    python tools/make_card.py --first Ada --last Lovelace \
        --title "Lead Engineer" --company "Analytical Engines" \
        --phone +15551234567 --email ada@example.com \
        --url https://example.github.io/card/

Start from an existing card and change one thing:

    python tools/make_card.py --from my_card.txt --phone +15559876543
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import flipper_card as fc  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build a Flipper NFC Business Card file.",
        epilog="Empty fields are left off the card entirely.",
    )
    parser.add_argument(
        "--from",
        dest="source",
        type=Path,
        help="existing my_card.txt to start from; other flags override it",
    )
    parser.add_argument("--first", help="first name")
    parser.add_argument("--last", help="last name")
    parser.add_argument("--title", help="job title")
    parser.add_argument("--company", help="company")
    parser.add_argument("--phone", help="phone, ideally in +country format")
    parser.add_argument("--email", help="email address")
    parser.add_argument("--url", help="website — this is what URL share mode puts on the tag")
    parser.add_argument("--note", help="free-text note")
    parser.add_argument(
        "--share-mode",
        choices=sorted(fc.SHARE_MODES),
        default=None,
        help="which NDEF records go on the tag (default: url, what iPhones read)",
    )
    parser.add_argument(
        "--out", type=Path, default=Path("my_card.txt"), help="output path (default: my_card.txt)"
    )
    args = parser.parse_args()

    fields: dict[str, str] = {}
    share_mode = fc.SHARE_MODES["url"]

    if args.source:
        if not args.source.is_file():
            raise SystemExit(f"{args.source}: no such file")
        fields = fc.read_card(args.source)
        if "share_mode" in fields:
            try:
                share_mode = int(fields.pop("share_mode"))
            except ValueError:
                pass

    for field in fc.FIELDS:
        value = getattr(args, field, None)
        if value is not None:
            fields[field] = value

    if args.share_mode is not None:
        share_mode = fc.SHARE_MODES[args.share_mode]

    if not any(fields.get(f) for f in fc.FIELDS):
        parser.error("give at least one field, e.g. --first Ada")

    for warning in fc.check_lengths(fields):
        print(f"warning: {warning}", file=sys.stderr)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    fc.write_card(args.out, fields, share_mode)

    print(f"wrote {args.out}\n")
    print(f"  {fc.display_name(fields)}")
    for field in fc.FIELDS:
        value = fields.get(field)
        if value:
            print(f"  {field + ':':<9} {value}")
    print(f"  {'share:':<9} {fc.SHARE_MODE_NAMES.get(share_mode, share_mode)}")

    if share_mode in (fc.SHARE_MODES["url"], fc.SHARE_MODES["both"]) and not fields.get("url"):
        print(
            "\nwarning: share mode needs a URL but none is set — the Share screen "
            "will say there is nothing to share.",
            file=sys.stderr,
        )

    print("\nCopy it to the Flipper at:")
    print("  /ext/apps_data/nfc_business_card/my_card.txt")
    print("then restart the app (it loads the card at startup).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
