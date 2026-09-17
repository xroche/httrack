#!/usr/bin/env python3
"""Black-box integration coverage for the metadata-only catalog CLI."""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) != 5:
        return 2
    cli = Path(sys.argv[1])
    seeder = Path(sys.argv[2])
    fixture_root = Path(sys.argv[3])
    legacy_cli = Path(sys.argv[4])

    with tempfile.TemporaryDirectory(prefix="httrack-catalog-cli-") as directory:
        catalog = Path(directory) / "catalog.sqlite3"

        def run(*arguments: str, ok: bool = True, machine: bool = True):
            command = [str(cli), "--catalog", str(catalog)]
            if machine:
                command.append("--json")
            command.extend(arguments)
            completed = subprocess.run(
                command, check=False, text=True, capture_output=True,
                env={**os.environ, "LC_ALL": "C.UTF-8"},
            )
            if ok and completed.returncode != 0:
                raise AssertionError(
                    f"command failed: {command!r}\nstdout={completed.stdout}\n"
                    f"stderr={completed.stderr}"
                )
            if not ok and completed.returncode == 0:
                raise AssertionError(f"command unexpectedly succeeded: {command!r}")
            if machine and completed.stdout:
                return completed, json.loads(completed.stdout)
            return completed, None

        _, initialized = run("init")
        assert initialized == {
            "catalog": str(catalog),
            "metadata_only": True,
            "schema_version": 4,
        }

        _, added = run(
            "source", "add", "--adapter", "yotsuba", "--board", "safe",
            "--name", "Fixture source",
        )
        assert added["id"] == 1 and added["metadata_only"] is True

        secret_ref = "env:TOP_SECRET_DO_NOT_PRINT"
        _, second = run(
            "source", "add", "--adapter", "yotsuba", "--url",
            "https://a.4cdn.org", "--board", "plain", "--name",
            "Credential reference fixture", "--credential-ref", secret_ref,
            "--disabled",
        )
        # Same adapter+origin updates the source rather than creating a duplicate.
        assert second["id"] == 1
        _, inspected = run("source", "inspect", "1")
        assert inspected["credential_configured"] is True
        assert secret_ref not in json.dumps(inspected)
        # Re-add the intended sync configuration and clear the credential reference.
        _, restored = run(
            "source", "add", "--adapter", "yotsuba", "--board", "safe",
            "--name", "Fixture source",
        )
        assert restored["id"] == 1

        _, dry_run = run("sync", "--source", "1", "--dry-run")
        assert dry_run["dry_run"] is True and dry_run["metadata_only"] is True
        _, board_dry_run = run(
            "sync", "--source", "1", "--board", "safe", "--dry-run"
        )
        assert board_dry_run["board_count"] == 1
        _, all_dry_run = run("sync", "--all", "--dry-run")
        assert all_dry_run == {
            "dry_run": True,
            "metadata_only": True,
            "source_ids": [1],
        }
        _, synced = run(
            "sync", "--source", "1", "--fixture-dir", str(fixture_root),
        )
        assert synced["media_bodies_requested"] == 0
        assert synced["posts_committed"] == 5
        assert synced["threads_fetched"] == 2
        _, sync_status = run("sync", "--source", "1", "--status")
        assert sync_status["source_id"] == 1
        assert sync_status["successful_cursor_count"] > 0
        _, all_synced = run("sync", "--all", "--fixture-dir", str(fixture_root))
        assert all_synced["metadata_only"] is True
        assert all_synced["results"] == [
            {"media_bodies_requested": 0, "source_id": 1, "threads_fetched": 0}
        ]

        subprocess.run(
            [str(seeder), "seed", str(catalog), "1"], check=True,
            text=True, capture_output=True,
        )

        _, empty = run("search", "collections", "--query", "no-such-result")
        assert empty["collections"] == []

        pages = []
        for offset, expected in ((0, 50), (50, 50), (100, 28)):
            _, page = run(
                "search", "collections", "--limit", "50", "--offset",
                str(offset),
            )
            assert len(page["collections"]) == expected
            pages.extend(item["id"] for item in page["collections"])
        assert len(pages) == len(set(pages)) == 128

        invalid, _ = run(
            "search", "collections", "--min-width", "100", "--max-width",
            "10", ok=False,
        )
        assert invalid.stdout == "" and "invalid search filter" in invalid.stderr

        _, filtered = run(
            "search", "collections", "--tag", "hostile", "--rating", "safe",
            "--media-type", "image/*", "--min-width", "600", "--max-bytes",
            "4096", "--availability", "expired",
        )
        assert [item["remote_id"] for item in filtered["collections"]] == ["hostile/900"]

        _, created = run(
            "selection", "create", "hostile", "--source", "1",
            "--from-search", "--tag", "hostile",
        )
        assert created["immutable"] is True and created["collection_count"] == 1
        duplicate, _ = run(
            "selection", "create", "hostile", "--source", "1",
            "--from-search", "--tag", "hostile", ok=False,
        )
        assert duplicate.stdout == ""

        _, stats = run("selection", "stats", "hostile", "--source", "1")
        assert stats["collections"] == 1
        assert stats["posts"] == 1 and stats["media_variants"] == 3
        assert stats["expected_original_bytes"] == 6144
        assert stats["unavailable_items"] == 1
        assert stats["already_local_items"] == 1
        assert stats["duplicate_content_candidates"] == 1

        first_preview, preview = run(
            "selection", "preview", "hostile", "--source", "1"
        )
        second_preview, repeated = run(
            "selection", "preview", "hostile", "--source", "1"
        )
        assert first_preview.stdout == second_preview.stdout
        paths = [item["path"] for item in preview["paths"]]
        assert len(paths) == len(set(paths)) == 3
        assert preview == repeated
        for path in paths:
            assert len(path.encode("utf-8")) <= 1024
            assert all(part not in ("", ".", "..") for part in path.split("/"))
            assert all(len(part.encode("utf-8")) <= 255 for part in path.split("/"))
            assert "CON?" not in path and "../" not in path
        assert any("Café" in path for path in paths)

        subprocess.run(
            [str(seeder), "mutate", str(catalog), "1"], check=True,
            text=True, capture_output=True,
        )
        _, unchanged = run("selection", "stats", "hostile", "--source", "1")
        assert unchanged["posts"] == 1 and unchanged["media_variants"] == 3
        _, refreshed = run("selection", "refresh", "hostile", "--source", "1")
        assert refreshed["refreshed"] is True
        _, changed = run("selection", "stats", "hostile", "--source", "1")
        assert changed["posts"] == 2 and changed["media_variants"] == 4

        first_sources, source_list = run("source", "list")
        second_sources, same_source_list = run("source", "list")
        assert first_sources.stdout == second_sources.stdout
        assert source_list == same_source_list

        remove_without_confirmation, _ = run(
            "source", "remove", "1", ok=False
        )
        assert "requires --yes" in remove_without_confirmation.stderr
        _, removed = run("source", "remove", "1", "--yes")
        assert removed["downloaded_files_deleted"] is False
        _, hidden = run("source", "list")
        assert hidden["sources"] == []
        _, visible = run("source", "list", "--include-removed")
        assert visible["sources"][0]["removed"] is True
        _, enabled = run("source", "enable", "1")
        assert enabled["action"] == "enable"

        posts, post_results = run(
            "search", "posts", "--rating", "safe", "--created-from",
            "1700000000", "--media-type", "image/*", "--min-width", "600",
        )
        assert posts.returncode == 0 and post_results["posts"]

        legacy = subprocess.run(
            [str(legacy_cli), "--version"], check=False,
            text=True, capture_output=True,
        )
        assert legacy.returncode == 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
