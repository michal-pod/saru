#!/usr/bin/env python3
"""Create and verify a detached GPG signature for a release artifact."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def run(command: list[str]) -> None:
    print(f"+ {subprocess.list2cmdline(command)}")
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=Path)
    parser.add_argument("signature", type=Path)
    args = parser.parse_args()

    if not args.artifact.is_file():
        raise RuntimeError(f"Release artifact does not exist: {args.artifact}")

    signing_key = os.environ.get("GPG_SIGNING_KEY")
    if not signing_key:
        raise RuntimeError("GPG_SIGNING_KEY must identify the release signing key.")

    gpg = shutil.which("gpg")
    if gpg is None:
        raise RuntimeError("Could not find gpg in PATH.")

    args.signature.parent.mkdir(parents=True, exist_ok=True)
    run([gpg, "--batch", "--yes", "--armor", "--detach-sign", "--local-user", signing_key,
         "--output", str(args.signature), str(args.artifact)])
    run([gpg, "--verify", str(args.signature), str(args.artifact)])
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
