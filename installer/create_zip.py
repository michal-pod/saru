import json, zipfile, pathlib, sys

manifest_path = pathlib.Path(sys.argv[1])
out_zip = pathlib.Path(sys.argv[2])

data = json.loads(manifest_path.read_text(encoding='utf-8'))

out_zip.parent.mkdir(parents=True, exist_ok=True)

with zipfile.ZipFile(out_zip, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in data['files']:
        p = pathlib.Path(f)
        z.write(p, 'SKYM/' + p.name)

    
    
