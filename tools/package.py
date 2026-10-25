import hashlib,json,zipfile
from pathlib import Path
r=Path(__file__).resolve().parents[1];p=r/'dist/MHGU Iai Spirit Slash'
m=json.loads((r/'research/build-p1.json').read_text());expected=json.loads((r/'validation/release-files.json').read_text())
actual={f.relative_to(p).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in p.rglob('*') if f.is_file()}
assert actual==m['files']
assert actual==expected,'Rebuilt addon differs from v1.0.0; review changes before packaging'
with zipfile.ZipFile(r/'dist/MHGU_LS_Spirit_Mod_v1.0.0.zip','w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for rel in sorted(actual):
        info=zipfile.ZipInfo('MHGU LS Spirit Mod/'+rel,date_time=(2026,10,24,12,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,(p/rel).read_bytes())
print('Compilação idêntica à versão publicada; ZIP em dist/.')
