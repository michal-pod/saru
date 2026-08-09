import json
import pathlib
import sys
import zipfile

manifest_path = pathlib.Path(sys.argv[1])
out_zip = pathlib.Path(sys.argv[2])

data = json.loads(manifest_path.read_text(encoding='utf-8'))

out_zip.parent.mkdir(parents=True, exist_ok=True)

with zipfile.ZipFile(out_zip, 'w', zipfile.ZIP_DEFLATED) as z:
    for entry in data['files']:
        if not entry.get('archive', False):
            continue

        if entry.get('type') == 'binary':
            p = pathlib.Path(entry['output_directory']) / entry['name']
        elif entry.get('type') == 'file':
            p = pathlib.Path(entry['source'])
        else:
            raise RuntimeError(f"Unknown manifest file type: {entry.get('type')}")

        if not p.is_file():
            raise RuntimeError(f"Archive input does not exist: {p}")
        z.write(p, 'SARU/' + p.name)

    
    
