#!/usr/bin/env python3
"""
Generate a hosted contact page from a Flipper business card.

iOS ignores vCard MIME records when it reads a tag in the background — it only
acts on a URI record. So the tag carries a URL, and the URL has to lead
somewhere that works on a phone. This builds that somewhere: a self-contained
page plus the .vcf it links to, ready to publish on GitHub Pages.

Reads the same my_card.txt the app writes, so the page and the tag can't drift:

    python tools/make_contact_page.py --card my_card.txt

Or supply the fields directly:

    python tools/make_contact_page.py --first Ada --last Lovelace \
        --email ada@example.com --phone +15551234567

Writes docs/index.html and docs/contact.vcf.
"""

from __future__ import annotations

import argparse
import html
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import flipper_card as fc  # noqa: E402

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{name}</title>
<style>
  :root {{
    color-scheme: light dark;
    --bg: #fff; --fg: #111; --muted: #666; --line: #e5e5e5; --accent: #0a6cff;
  }}
  @media (prefers-color-scheme: dark) {{
    :root {{ --bg: #111; --fg: #f2f2f2; --muted: #9a9a9a; --line: #2a2a2a; --accent: #4d93ff; }}
  }}
  * {{ box-sizing: border-box; }}
  body {{
    margin: 0; padding: 2.5rem 1.25rem; background: var(--bg); color: var(--fg);
    font: 16px/1.5 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    display: flex; justify-content: center;
  }}
  main {{ width: 100%; max-width: 26rem; }}
  h1 {{ font-size: 1.75rem; line-height: 1.2; margin: 0 0 .25rem; }}
  .sub {{ color: var(--muted); margin: 0; }}
  .save {{
    display: block; margin: 2rem 0; padding: .95rem 1rem; border-radius: .7rem;
    background: var(--accent); color: #fff; font-weight: 600; text-align: center;
    text-decoration: none;
  }}
  .hint {{ color: var(--muted); font-size: .8rem; text-align: center; margin: -1.4rem 0 2rem; }}
  ul {{ list-style: none; margin: 0; padding: 0; border-top: 1px solid var(--line); }}
  li {{ border-bottom: 1px solid var(--line); }}
  li a, li span {{
    display: flex; gap: 1rem; padding: .85rem .25rem;
    color: var(--fg); text-decoration: none;
  }}
  li a {{ color: var(--accent); }}
  .label {{ color: var(--muted); min-width: 5rem; flex-shrink: 0; }}
  .value {{ overflow-wrap: anywhere; }}
  footer {{ margin-top: 2.5rem; color: var(--muted); font-size: .75rem; text-align: center; }}
</style>
</head>
<body>
<main>
  <h1>{name}</h1>
  {subtitle}
  <a class="save" href="{vcf}">Add to Contacts</a>
  <p class="hint">On iPhone: tap, then use the share icon &rarr; Contacts.</p>
  <ul>
{rows}  </ul>
  <footer>Shared from a Flipper Zero</footer>
</main>
</body>
</html>
"""


def build_page(fields: dict[str, str], vcf_name: str) -> str:
    # Escape the parts, then join with the separator entity — escaping the
    # joined string would turn "&middot;" into "&amp;middot;".
    subtitle_parts = [
        html.escape(p) for p in (fields.get("title"), fields.get("company")) if p
    ]
    subtitle = (
        f'<p class="sub">{" &middot; ".join(subtitle_parts)}</p>' if subtitle_parts else ""
    )

    rows = []
    for label, field, scheme in (
        ("Phone", "phone", "tel:"),
        ("Email", "email", "mailto:"),
        ("Website", "url", ""),
        ("Note", "note", None),
    ):
        value = fields.get(field)
        if not value:
            continue

        safe = html.escape(value)
        if scheme is None:
            rows.append(
                f'    <li><span><span class="label">{label}</span>'
                f'<span class="value">{safe}</span></span></li>\n'
            )
        else:
            href = html.escape(scheme + value, quote=True)
            rows.append(
                f'    <li><a href="{href}"><span class="label">{label}</span>'
                f'<span class="value">{safe}</span></a></li>\n'
            )

    return PAGE.format(
        name=html.escape(fc.display_name(fields)),
        subtitle=subtitle,
        vcf=html.escape(vcf_name, quote=True),
        rows="".join(rows),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a hosted contact page from a card.")
    parser.add_argument("--card", type=Path, help="my_card.txt saved by the Flipper app")
    for field in fc.FIELDS:
        parser.add_argument(f"--{field}", help=f"override the {field} field")
    parser.add_argument(
        "--out", type=Path, default=Path("docs"), help="output directory (default: docs)"
    )
    parser.add_argument(
        "--vcf-name", default="contact.vcf", help="vCard filename (default: contact.vcf)"
    )
    args = parser.parse_args()

    fields: dict[str, str] = {}
    if args.card:
        if not args.card.is_file():
            raise SystemExit(f"{args.card}: no such file")
        fields = fc.read_card(args.card)
        fields.pop("share_mode", None)

    for field in fc.FIELDS:
        value = getattr(args, field, None)
        if value:
            fields[field] = value

    if not fields:
        parser.error("give --card or at least one field, e.g. --first Ada")

    args.out.mkdir(parents=True, exist_ok=True)
    vcf_path = args.out / args.vcf_name
    index_path = args.out / "index.html"

    vcf_path.write_text(fc.build_vcard(fields), encoding="utf-8", newline="")
    index_path.write_text(build_page(fields, args.vcf_name), encoding="utf-8")

    print(f"wrote {index_path}")
    print(f"wrote {vcf_path}")
    print()
    print(f"Contact: {fc.display_name(fields)}")
    print("Publish docs/ with GitHub Pages, then put that URL in the app's Website field.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
