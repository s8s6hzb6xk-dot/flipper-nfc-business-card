#!/usr/bin/env python3
"""
Generate a hosted contact page from a Flipper business card.

iOS ignores vCard MIME records when it reads a tag in the background — it only
acts on a URI record. So the tag carries a URL, and the URL has to lead
somewhere that works on a phone. This builds that somewhere: a self-contained
page plus the .vcf it links to, ready to publish on GitHub Pages.

Reads the same my_card.txt the app writes, so the page and the tag can't drift:

    python tools/make_contact_page.py --card my_card.txt

The Flipper caps every field at 95 characters. Anything longer — a bio, a list
of skills — lives here instead, either as flags or in page_extras.txt:

    About: Founded 2026.
    Skills: Research, Public speaking, Leadership

Writes docs/index.html and docs/contact.vcf.
"""

from __future__ import annotations

import argparse
import html
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import flipper_card as fc  # noqa: E402

DEFAULT_EXTRAS = Path("page_extras.txt")

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{name}</title>
<style>
  :root {{
    color-scheme: light dark;
    --bg: #fff; --fg: #111; --muted: #666; --line: #e5e5e5;
    --accent: #0a6cff; --chip: #f0f2f5;
  }}
  @media (prefers-color-scheme: dark) {{
    :root {{
      --bg: #111; --fg: #f2f2f2; --muted: #9a9a9a; --line: #2a2a2a;
      --accent: #4d93ff; --chip: #1e1e1e;
    }}
  }}
  * {{ box-sizing: border-box; }}
  body {{
    margin: 0; padding: 2.5rem 1.25rem; background: var(--bg); color: var(--fg);
    font: 16px/1.5 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    display: flex; justify-content: center;
  }}
  main {{ width: 100%; max-width: 26rem; }}
  h1 {{ font-size: 1.75rem; line-height: 1.2; margin: 0 0 .25rem; }}
  h2 {{
    font-size: .75rem; text-transform: uppercase; letter-spacing: .08em;
    color: var(--muted); margin: 2rem 0 .6rem; font-weight: 600;
  }}
  .sub {{ color: var(--muted); margin: 0; }}
  .save {{
    display: block; margin: 2rem 0; padding: .95rem 1rem; border-radius: .7rem;
    background: var(--accent); color: #fff; font-weight: 600; text-align: center;
    text-decoration: none;
  }}
  .hint {{ color: var(--muted); font-size: .8rem; text-align: center; margin: -1.4rem 0 2rem; }}
  ul.rows {{ list-style: none; margin: 0; padding: 0; border-top: 1px solid var(--line); }}
  ul.rows li {{ border-bottom: 1px solid var(--line); }}
  ul.rows a, ul.rows span {{
    display: flex; gap: 1rem; padding: .85rem .25rem;
    color: var(--fg); text-decoration: none;
  }}
  ul.rows a {{ color: var(--accent); }}
  .label {{ color: var(--muted); min-width: 5rem; flex-shrink: 0; }}
  .value {{ overflow-wrap: anywhere; }}
  p.about {{ margin: 0; }}
  ul.chips {{ list-style: none; margin: 0; padding: 0; display: flex; flex-wrap: wrap; gap: .4rem; }}
  ul.chips li {{
    background: var(--chip); border-radius: 1rem; padding: .35rem .75rem; font-size: .875rem;
  }}
  footer {{ margin-top: 2.5rem; color: var(--muted); font-size: .75rem; text-align: center; }}
</style>
</head>
<body>
<main>
  <h1>{name}</h1>
  {subtitle}
  <a class="save" href="{vcf}">Add to Contacts</a>
  <p class="hint">On iPhone: tap, then use the share icon &rarr; Contacts.</p>
  <ul class="rows">
{rows}  </ul>
{about}{skills}  <footer>Shared from a Flipper Zero</footer>
</main>
</body>
</html>
"""


def read_extras(path: Path) -> dict[str, str]:
    """Parse a simple 'Key: value' extras file. Unknown keys are ignored."""
    extras: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        key, sep, value = line.partition(":")
        if not sep:
            continue
        key = key.strip().lower()
        if key in ("about", "skills"):
            extras[key] = value.strip()
    return extras


def split_skills(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def build_page(fields: dict[str, str], vcf_name: str, extras: dict[str, str]) -> str:
    # Escape the parts, then join with the separator entity — escaping the
    # joined string would turn "&middot;" into "&amp;middot;".
    subtitle_parts = [html.escape(p) for p in (fields.get("title"), fields.get("company")) if p]
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

    about = ""
    if extras.get("about"):
        about = f'  <h2>About</h2>\n  <p class="about">{html.escape(extras["about"])}</p>\n'

    skills = ""
    if extras.get("skills"):
        items = "".join(
            f"    <li>{html.escape(item)}</li>\n" for item in split_skills(extras["skills"])
        )
        if items:
            skills = f'  <h2>Skills</h2>\n  <ul class="chips">\n{items}  </ul>\n'

    return PAGE.format(
        name=html.escape(fc.display_name(fields)),
        subtitle=subtitle,
        vcf=html.escape(vcf_name, quote=True),
        rows="".join(rows),
        about=about,
        skills=skills,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a hosted contact page from a card.")
    parser.add_argument("--card", type=Path, help="my_card.txt saved by the Flipper app")
    for field in fc.FIELDS:
        parser.add_argument(f"--{field}", help=f"override the {field} field")
    parser.add_argument("--about", help="short bio paragraph; page only, no length limit")
    parser.add_argument("--skills", help="comma-separated list; page only, no length limit")
    parser.add_argument(
        "--extras",
        type=Path,
        default=None,
        help=f"'Key: value' file with About/Skills (default: {DEFAULT_EXTRAS} if present)",
    )
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

    extras: dict[str, str] = {}
    extras_path = args.extras or (DEFAULT_EXTRAS if DEFAULT_EXTRAS.is_file() else None)
    if extras_path:
        if not extras_path.is_file():
            raise SystemExit(f"{extras_path}: no such file")
        extras = read_extras(extras_path)
    if args.about:
        extras["about"] = args.about
    if args.skills:
        extras["skills"] = args.skills

    # The page's vCard carries the long-form text the Flipper's 95-char NOTE
    # cannot hold. In URL share mode the tag only emits the link anyway.
    vcard_fields = dict(fields)
    note_parts = [p for p in (fields.get("note"), extras.get("about")) if p]
    if extras.get("skills"):
        note_parts.append("Skills: " + ", ".join(split_skills(extras["skills"])))
    if note_parts:
        vcard_fields["note"] = " ".join(note_parts)

    args.out.mkdir(parents=True, exist_ok=True)
    vcf_path = args.out / args.vcf_name
    index_path = args.out / "index.html"

    vcf_path.write_text(fc.build_vcard(vcard_fields), encoding="utf-8", newline="")
    index_path.write_text(build_page(fields, args.vcf_name, extras), encoding="utf-8")

    print(f"wrote {index_path}")
    print(f"wrote {vcf_path}")
    if extras_path:
        print(f"extras from {extras_path}")
    print()
    print(f"Contact: {fc.display_name(fields)}")
    print(f"Publish {args.out}/ with GitHub Pages, then put that URL in the app's Website field.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
