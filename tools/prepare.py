"""Build inputs come from the released addon, never from another local project."""
import hashlib,json,os,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PRIOR=ROOT/'reference'
def main():
    build=ROOT/'build';build.mkdir(exist_ok=True)
    (ROOT/'research').mkdir(exist_ok=True)
    source=Path(os.environ.get('MHGU_LS_ADDON',str(ROOT/'inputs/MHGU_LS_Spirit_Mod_v1.0.3.zip')))
    assert hashlib.sha256(source.read_bytes()).hexdigest()=='12ade71ce579a0a08541ce0dc0e8379ec27a9922881c6953cb6a37fab00061df','Wrong released input ZIP'
    expected=json.loads((ROOT/'validation/release-files.json').read_text())
    with zipfile.ZipFile(source) as z:
        for rel,digest in expected.items():
            data=z.read('MHGU LS Spirit Mod/'+rel)
            assert hashlib.sha256(data).hexdigest()==digest,rel
            if rel.startswith('romfs/') or rel=='LEIA-ME.txt':
                dest=ROOT/'dist/MHGU Iai Spirit Slash'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
    for p in (PRIOR/'generated').iterdir():shutil.copyfile(p,build/p.name)
