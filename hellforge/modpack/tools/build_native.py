"""Build the C and C++ runtime with the installed SDK and pinned Zig compiler.

Usage: python tools/build_native.py --install PATH_TO_HELLFORGE
Produces the manifest's game-pinned prebuilt DLL; never deploys or launches.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile
def build(mod,install):
 mod=Path(mod).resolve();install=Path(install).resolve();zig=install/'tools/zig/zig.exe';sdk=install/'app/sdk'
 assert zig.is_file() and (sdk/'hellforge_mod.h').is_file()
 with tempfile.TemporaryDirectory(prefix='arsenal-native-') as temp:
  temp=Path(temp);objects=[]
  for language,source,standard in [('cc','src/native_pack.c','c11'),('c++','src/primary_hammer/mod.cpp','c++17'),('c++','src/primary_hammer/combat.cpp','c++17')]:
   obj=temp/(Path(source).stem+'.o');args=[zig,language,'-c','-O2','-std='+standard,'-Wall','-Werror','-Wno-unused-parameter','-I',sdk,mod/source,'-o',obj]
   subprocess.run(list(map(str,args)),check=True);objects.append(obj)
  output=mod/'super_earth_arsenal.dll'
  subprocess.run(list(map(str,[zig,'c++','-shared','-O2','-s',*objects,'-o',output,'-luser32'])),check=True)
 result=dict(dll=output.name,sha256=hashlib.sha256(output.read_bytes()).hexdigest(),game='25480438',
  source_sha256={p.relative_to(mod).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (mod/'src').rglob('*') if p.is_file()})
 (mod/'native-build.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--install',required=True);args=parser.parse_args()
 print(json.dumps(build(Path(__file__).resolve().parents[1],args.install),indent=2))
