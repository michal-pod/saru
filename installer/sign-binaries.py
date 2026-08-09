#!/usr/bin/env python3
"""Copy, Authenticode-sign and verify files listed in a release manifest.

The signing certificate is selected from the Windows certificate store using
the same environment variables as gpg-afd:

* CODESIGN_SHA selects a certificate by SHA-1 thumbprint.
* CODESIGN_CN selects it by subject name when CODESIGN_SHA is unset.
* CODESIGN_ISSUER optionally restricts the selected certificate.

If neither CODESIGN_SHA nor CODESIGN_CN is set, SignTool selects a suitable
certificate automatically.  A successful signing operation is not enough:
every generated file is verified with the Authenticode policy before the
target succeeds.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


TIMESTAMP_URL = "http://time.certum.pl/"


def run(command: list[str]) -> None:
    print(f"+ {subprocess.list2cmdline(command)}")
    subprocess.run(command, check=True)


def find_tool(name: str) -> str:
    tool = shutil.which(name)
    if tool is None:
        raise RuntimeError(f"Could not find {name} in PATH.")
    return tool


def require_file(path: Path) -> None:
    if not path.is_file():
        raise RuntimeError(f"Required file does not exist: {path}")


def validate_output_name(name: str) -> None:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", name):
        raise RuntimeError(f"Invalid output file name: {name}")


def sign_file(signtool: str, path: Path) -> None:
    command = [signtool, "sign", "/fd", "SHA256"]

    certificate_sha = os.environ.get("CODESIGN_SHA")
    certificate_cn = os.environ.get("CODESIGN_CN")
    certificate_issuer = os.environ.get("CODESIGN_ISSUER")

    if certificate_sha:
        command.extend(["/sha1", certificate_sha])
    elif certificate_cn:
        command.extend(["/n", certificate_cn])
    else:
        command.append("/a")

    if certificate_issuer:
        command.extend(["/i", certificate_issuer])

    command.extend(["/tr", TIMESTAMP_URL, "/td", "SHA256", str(path)])
    run(command)
    run([signtool, "verify", "/pa", "/v", str(path)])


def process_manifest(manifest_path: Path, group: str, stamp_path: Path) -> None:
    require_file(manifest_path)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        entries = manifest["files"]
    except (json.JSONDecodeError, KeyError, TypeError) as error:
        raise RuntimeError(f"Invalid signing manifest {manifest_path}: {error}") from error

    selected_entries = [
        entry for entry in entries
        if entry.get("type") == "binary" and entry.get("group") == group
    ]
    if not selected_entries:
        raise RuntimeError(f"Signing manifest does not contain files for group: {group}")

    signtool = find_tool("signtool")
    output_paths: set[Path] = set()
    for entry in selected_entries:
        try:
            source = Path(entry["source"])
            output_directory = Path(entry["output_directory"])
            output_name = entry["name"]
        except (KeyError, TypeError) as error:
            raise RuntimeError(f"Invalid signing manifest entry: {entry}") from error

        validate_output_name(output_name)
        require_file(source)
        output_directory.mkdir(parents=True, exist_ok=True)
        destination = (output_directory / output_name).resolve()
        if destination.parent != output_directory.resolve():
            raise RuntimeError(f"Signing output escapes its configured directory: {destination}")
        if destination in output_paths:
            raise RuntimeError(f"Duplicate signing output: {destination}")
        output_paths.add(destination)

        shutil.copy2(source, destination)
        sign_file(signtool, destination)

    stamp_path.parent.mkdir(parents=True, exist_ok=True)
    stamp_path.write_text(f"Signed {len(selected_entries)} {group} file(s).\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--group", required=True)
    parser.add_argument("--stamp", required=True, type=Path)
    args = parser.parse_args()

    process_manifest(args.manifest, args.group, args.stamp)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
