"""Compose a new native source selection. Python 3.10+, standard library only.

Edit pack-config.json, then run:
  python tools/configure_pack.py --config pack-config.json --out NEW_FOLDER
No deployment, compilation, game data, settings changes or game process access.
"""
import argparse
import json
from pathlib import Path
import shutil


def compose(mod, config_path, out):
    mod=Path(mod).resolve();out=Path(out).resolve()
    if out.exists():raise ValueError('Refusing to overwrite an existing output')
    if out==mod or mod in out.parents:raise ValueError('Output must be outside the source mod folder')
    catalog=json.loads((mod/'tools/selection-catalog.json').read_text(encoding='utf-8'))
    config=json.loads(Path(config_path).read_text(encoding='utf-8'))
    if config.get('schema')!=1 or config.get('baseline')!='2.4.1':raise ValueError('Unknown config schema/baseline')
    options=config.get('options');known=catalog['options']
    if not isinstance(options,dict) or set(options)!=set(known) or len(known)!=25:
        raise ValueError('Option keys must match all 25 option IDs')
    if not all(type(value)is bool for value in options.values()):raise ValueError('Options must be JSON booleans')
    flag=config.get('flag')
    if not isinstance(flag,dict) or set(flag)!={'normal','durable'}:raise ValueError('Invalid Flag preset')
    normal,durable=flag['normal'],flag['durable']
    if type(normal)is not int or type(durable)is not int or normal not in catalog['flag_normal'] or durable not in (normal,normal//2):
        raise ValueError('Unsupported Flag preset')
    lines=['#ifndef SUPER_EARTH_NATIVE_PACK_CONFIG_H','#define SUPER_EARTH_NATIVE_PACK_CONFIG_H']
    lines += [f"#define {spec['macro']} {int(options[option])}" for option,spec in known.items()]
    lines += [f'#define HF_PORT_FLAG_NORMAL_DAMAGE {normal}',f'#define HF_PORT_FLAG_DURABLE_DAMAGE {durable}', '#endif','']
    edits=[edit['line'] for edit in catalog['edits'] if options[edit['option']]]
    edit_header='/* Generated native selection. */\nstatic const HfPortEdit HF_PORT_EDITS[] = {\n'
    edit_header+='\n'.join(edits if edits else ['    {0},'])
    edit_header+=f'\n}};\n#define HF_PORT_EDIT_COUNT {len(edits)}u\n'
    archives={bundle:rows for bundle,rows in catalog['asset_archives'].items()
              if any(options[option] for option in catalog['asset_owners'][bundle])}
    # Only an explicit source/recipe allowlist is copied. Existing local data,
    # binaries, evidence and installation files never enter the new selection.
    required=[mod/'hellforge-mod.json',mod/'src',mod/'tools']
    if not all(p.exists() for p in required):raise ValueError('Source mod is incomplete')
    if any(p.suffix.lower() in ('.dll','.exe','.zip') or '.patch_' in p.name
           for folder in ('src','tools') for p in (mod/folder).rglob('*') if p.is_file()):
        raise ValueError('Unexpected binary or game archive in source/recipes')
    out.mkdir(parents=True)
    ignore=shutil.ignore_patterns('__pycache__','*.pyc')
    shutil.copytree(mod/'src',out/'src',ignore=ignore);shutil.copytree(mod/'tools',out/'tools',ignore=ignore)
    shutil.copy2(mod/'hellforge-mod.json',out/'hellforge-mod.json')
    for name in ('INSTALL.md','PLAYER-GUIDE.md'):
        if (mod/name).is_file():shutil.copy2(mod/name,out/name)
    (out/'data').mkdir()
    (out/'src/pack_config.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
    (out/'src/port_edits.generated.h').write_text(edit_header,encoding='utf-8',newline='\n')
    (out/'pack-config.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
    (out/'expected-asset-resources.json').write_text(json.dumps({'schema':1,'archives':archives},indent=2)+'\n',encoding='utf-8')
    result={'enabled':[option for option,value in options.items() if value],'scalar_edits':len(edits),
            'expected_asset_instances':sum(len(rows) for rows in archives.values()),'staged':False}
    (out/'selection-receipt.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(compose(Path(__file__).resolve().parents[1],args.config,args.out),indent=2))
