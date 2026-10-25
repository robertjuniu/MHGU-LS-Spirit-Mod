"""Build inputs come from the released addon, never from another local project."""
import hashlib,json,os,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PRIOR=ROOT/'reference'
def main():
    build=ROOT/'build';build.mkdir(exist_ok=True)
    (ROOT/'research').mkdir(exist_ok=True)
    source=Path(os.environ.get('MHGU_LS_ADDON',str(ROOT/'inputs/MHGU_LS_Spirit_Mod_v1.0.2.zip')))
    assert hashlib.sha256(source.read_bytes()).hexdigest()=='8d445867ed614a35fc6401bc0b446a7e7b7fbb456968e48cc52ebc9e499992b4','Wrong released input ZIP'
    expected=json.loads((ROOT/'validation/release-files.json').read_text())
    with zipfile.ZipFile(source) as z:
        for rel,digest in expected.items():
            data=z.read('MHGU LS Spirit Mod/'+rel)
            assert hashlib.sha256(data).hexdigest()==digest,rel
            if rel.startswith('romfs/') or rel=='LEIA-ME.txt':
                dest=ROOT/'dist/MHGU Iai Spirit Slash'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
    for p in (PRIOR/'generated').iterdir():shutil.copyfile(p,build/p.name)
