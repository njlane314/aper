#!/usr/bin/env python3

import importlib.util
import importlib.machinery
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "tools" / "fetch-encyclopedia-artwork"
LOADER = importlib.machinery.SourceFileLoader(
    "fetch_encyclopedia_artwork", str(SCRIPT)
)
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
assert SPEC is not None and SPEC.loader is not None
artwork = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = artwork
SPEC.loader.exec_module(artwork)


LICENCE_FOOTER = f"<footer><a href='{artwork.LICENCE}'>licence</a></footer>"


class DetailParserTest(unittest.TestCase):
    def test_normal_assets_and_vector_patch(self) -> None:
        detail = artwork.parse_detail(
            "<h3>Substitution Rule</h3>"
            "<img src='/img/rule.gif' alt='Rule Example'>"
            "<h3>Patch</h3>"
            "<img src='/img/patch.png' alt='Patch Example'>"
            "<a href='/img/patch.pdf'> download vectorformat Example </a>"
            + LICENCE_FOOTER
        )
        self.assertEqual(detail.rule, "/img/rule.gif")
        self.assertEqual(detail.patch, "/img/patch.png")
        self.assertEqual(detail.vector_patch, "/img/patch.pdf")

    def test_dual_assets(self) -> None:
        detail = artwork.parse_detail(
            "<h3>Dual Substitution Rule</h3>"
            "<img src='/img/dualrule.svg' alt='Dual Rule Example'>"
            "<h3>Dual Patch</h3>"
            "<img src='/img/dualpatch.svg' alt='Dual Patch Example'>"
            + LICENCE_FOOTER
        )
        self.assertEqual(detail.rule, "/img/dualrule.svg")
        self.assertEqual(detail.patch, "/img/dualpatch.svg")

    def test_missing_rule_is_allowed_but_missing_licence_is_not(self) -> None:
        detail = artwork.parse_detail(
            "<h3>Patch</h3><img src='/img/patch.jpg' alt='Patch Example'>"
            + LICENCE_FOOTER
        )
        self.assertIsNone(detail.rule)
        with self.assertRaises(artwork.ArtworkError):
            artwork.parse_detail(
                "<h3>Patch</h3><img src='/img/patch.jpg' alt='Patch Example'>"
            )

    def test_duplicate_assets_are_rejected(self) -> None:
        with self.assertRaises(artwork.ArtworkError):
            artwork.parse_detail(
                "<h3>Patch</h3><img src='/img/a.png' alt='Patch A'>"
                "<h3>Dual Patch</h3><img src='/img/b.png' alt='Dual Patch B'>"
                + LICENCE_FOOTER
            )


class ManifestTest(unittest.TestCase):
    def test_missing_rule_uses_sentinels_and_no_trailing_field(self) -> None:
        patch = artwork.LocalAsset(
            "https://tilings.math.uni-bielefeld.de/img/patch.pdf",
            ".build/catalogue/reference/example-patch.pdf",
            "a" * 64,
            "application/pdf",
            "application/pdf copied without modification",
        )
        manifest = artwork.render_manifest(
            [
                artwork.ManifestRow(
                    "example",
                    "https://tilings.math.uni-bielefeld.de/substitution/example/",
                    patch,
                    None,
                    "patch: copied; rule: rule asset not supplied by source",
                )
            ]
        )
        fields = manifest.splitlines()[1].split("\t")
        self.assertEqual(len(fields), len(artwork.MANIFEST_FIELDS))
        self.assertEqual(fields[6:10], ["-", "-", "-", "-"])
        self.assertNotEqual(fields[-1], "")

    def test_conversion_contract(self) -> None:
        self.assertEqual(artwork.output_format("image/gif").converter, "gif")
        self.assertEqual(artwork.output_format("image/svg+xml").converter, "svg")
        self.assertIsNone(artwork.output_format("application/pdf").converter)

    def test_gif_conversion_is_portable(self) -> None:
        programs = {"magick": "/usr/bin/magick", "convert": None, "sips": None}
        with mock.patch.object(
            artwork.shutil, "which", side_effect=lambda name: programs.get(name)
        ):
            converter, command = artwork.conversion_command(
                "gif", pathlib.Path("source.gif"), pathlib.Path("target.png")
            )
        self.assertEqual(converter, "magick")
        self.assertEqual(command, ["/usr/bin/magick", "source.gif[0]", "target.png"])

    def test_svg_conversion_falls_back_to_inkscape(self) -> None:
        programs = {"rsvg-convert": None, "inkscape": "/usr/bin/inkscape"}
        with mock.patch.object(
            artwork.shutil, "which", side_effect=lambda name: programs.get(name)
        ):
            converter, command = artwork.conversion_command(
                "svg", pathlib.Path("source.svg"), pathlib.Path("target.pdf")
            )
        self.assertEqual(converter, "inkscape")
        self.assertEqual(command[0], "/usr/bin/inkscape")


class OfflineFetchTest(unittest.TestCase):
    def test_cached_page_and_assets_need_no_network(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            cache_root = root / "cache"
            page_url = (
                "https://tilings.math.uni-bielefeld.de/substitution/example/"
            )
            page = (
                "<h3>Substitution Rule</h3>"
                "<img src='/img/example/rule.png' alt='Rule Example'>"
                "<h3>Patch</h3>"
                "<img src='/img/example/patch.png' alt='Patch Example'>"
                "<a href='/img/example/patch.pdf'>download vectorformat Example</a>"
                + LICENCE_FOOTER
            ).encode()
            page_path = cache_root / "pages" / "example.html"
            page_path.parent.mkdir(parents=True)
            page_path.write_bytes(page)
            (cache_root / "pages" / "example.json").write_text(
                json.dumps(
                    {
                        "charset": "utf-8",
                        "final_url": page_url,
                        "media_type": "text/html",
                        "sha256": hashlib.sha256(page).hexdigest(),
                        "source_url": page_url,
                    }
                )
            )

            def cache_asset(url: str, media_type: str, content: bytes) -> None:
                digest = hashlib.sha256(url.encode()).hexdigest()
                suffix = pathlib.PurePosixPath(
                    artwork.urllib.parse.urlsplit(url).path
                ).suffix
                asset_path = cache_root / "assets" / f"{digest}{suffix}"
                asset_path.parent.mkdir(parents=True, exist_ok=True)
                asset_path.write_bytes(content)
                (cache_root / "assets" / f"{digest}.json").write_text(
                    json.dumps(
                        {
                            "charset": "utf-8",
                            "final_url": url,
                            "media_type": media_type,
                            "sha256": hashlib.sha256(content).hexdigest(),
                            "source_url": url,
                        }
                    )
                )

            patch_url = (
                "https://tilings.math.uni-bielefeld.de/img/example/patch.pdf"
            )
            rule_url = (
                "https://tilings.math.uni-bielefeld.de/img/example/rule.png"
            )
            cache_asset(patch_url, "application/pdf", b"%PDF fixture\n")
            cache_asset(rule_url, "image/png", b"PNG fixture\n")

            cache = artwork.ResourceCache(
                cache_root, artwork.GentleFetcher(), offline=True, refresh=False
            )
            rows = artwork.fetch_artwork(
                [artwork.BankEntry("example", page_url)], cache, root / "assets"
            )
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0].patch.url, patch_url)
            self.assertEqual(rows[0].patch.media_type, "application/pdf")
            self.assertEqual(rows[0].rule.url, rule_url)
            self.assertIn("vectorformat preferred", rows[0].changes)
            self.assertEqual(len(rows[0].patch.sha256), 64)


if __name__ == "__main__":
    unittest.main()
