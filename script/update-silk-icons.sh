#!/usr/bin/env bash
# Refresh SARU's bundled Silk icons from Linux/WSL; this is not a build-time step.
# Dependencies (Debian/Ubuntu): git, librsvg2-bin, imagemagick.
# Usage: bash script/update-silk-icons.sh [output-directory]
# With no argument, updates resources/*.ico. app.ico is SARU's own artwork.

set -euo pipefail

usage() {
    printf 'Usage: %s [output-directory]\n' "${0##*/}"
    printf 'Requires git, rsvg-convert (librsvg2-bin), and ImageMagick.\n'
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    usage
    exit 0
fi
if (( $# > 1 )); then
    usage >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)
output_dir=${1:-"$project_dir/resources"}
upstream_url=https://github.com/Simandara/famfamfam-silk-svg.git

for command in git rsvg-convert; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'Missing command: %s. Install git and librsvg2-bin.\n' "$command" >&2
        exit 1
    fi
done
if command -v magick >/dev/null 2>&1; then
    imagemagick=(magick)
elif command -v convert >/dev/null 2>&1; then
    imagemagick=(convert)
else
    printf 'Missing ImageMagick (magick or convert).\n' >&2
    exit 1
fi

# Sizes for 16px icons at 100, 125, 150, 175, 200, 225, 250, 300,
# 350, and 400 percent scaling. Linux also uses a 32px base size.
small_sizes=(16 20 24 28 32 36 40 48 56 64)
linux_sizes=("${small_sizes[@]}" 72 80 96 112 128)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/saru-silk-icons.XXXXXX")
trap 'rm -rf -- "$work_dir"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

git clone --depth 1 -- "$upstream_url" "$work_dir/upstream"
upstream_commit=$(git -C "$work_dir/upstream" rev-parse HEAD)
mkdir -- "$work_dir/output" "$work_dir/png"

# Read the actual icon resources rather than importing the entire icon set.
mapfile -t icons < <(awk '$2 == "ICON" { gsub(/["\r]/, "", $3); print $3 }' \
    "$project_dir/resources/resources.rc")
silk_icons=()
svg_files=()
for filename in "${icons[@]}"; do
    stem=${filename%.ico}
    [[ $stem == app ]] && continue
    if [[ $stem == linux ]]; then
        source_name=Tux
    else
        source_name=${stem//_/ }
        source_name=${source_name^}
    fi
    svg="$work_dir/upstream/icons/$source_name.svg"
    if [[ ! -f $svg ]]; then
        printf 'Missing upstream SVG for %s: %s.svg\n' "$filename" "$source_name" >&2
        exit 1
    fi
    silk_icons+=("$filename")
    svg_files+=("$svg")
done
if (( ${#silk_icons[@]} == 0 )); then
    printf 'No Silk icons found in resources/resources.rc.\n' >&2
    exit 1
fi

for index in "${!silk_icons[@]}"; do
    filename=${silk_icons[index]}
    sizes=("${small_sizes[@]}")
    if [[ $filename == linux.ico ]]; then
        sizes=("${linux_sizes[@]}")
    fi
    printf 'Converting %s\n' "$filename"
    png_frames=()
    for size in "${sizes[@]}"; do
        png="$work_dir/png/${filename%.ico}-$size.png"
        rsvg-convert --width "$size" --height "$size" \
            --output "$png" "${svg_files[index]}"
        png_frames+=("$png")
    done
    "${imagemagick[@]}" "${png_frames[@]}" -alpha on -depth 8 \
        "ICO:$work_dir/output/$filename"
done

cp -- "$work_dir/upstream/LICENSE" "$work_dir/output/famfamfam-silk-svg.LICENSE.txt"
cat > "$work_dir/output/famfamfam-silk-svg.ATTRIBUTION.txt" <<EOF
FamFamFam Silk SVG icons by Simon (Simandara), based on Silk by Mark James.
Source: https://github.com/Simandara/famfamfam-silk-svg
Revision: $upstream_commit
License: Creative Commons Attribution 4.0 International (CC BY 4.0)
https://creativecommons.org/licenses/by/4.0/
Changes: SVG artwork rasterized into multi-resolution ICO files for SARU.
linux.ico is generated from Tux.svg; other names follow resources/resources.rc.
Sizes in pixels (16px icons): ${small_sizes[*]}
Sizes in pixels (linux.ico, 16px and 32px): ${linux_sizes[*]}
EOF

# Publish only after every source has rendered and every ICO has been written.
mkdir -p -- "$output_dir"
cp -- "$work_dir/output/"* "$output_dir/"
printf 'Updated %s icons in %s (upstream %s).\n' \
    "${#silk_icons[@]}" "$output_dir" "$upstream_commit"
