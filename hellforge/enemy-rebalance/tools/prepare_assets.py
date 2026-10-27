"""Prepare Enemy Rebalance animation assets from the owner's updated game data.

Python 3.10+, standard library only. Reads source game data and writes to a new
output folder. Does not deploy mods, change settings, or access a game process.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import subprocess

SOURCE_SHA = '78cd1a211e2c40dfb1f5f01bbb342677b963104a644d0fad8844c94cb233d52b'
OUTPUT_SHA = '850e0114e4f683811edc6d0807fb1eca459a04154714873edb5dc3232c775106'
ANIMATION_TYPE = 0x931E336D7646CC26
ANIMATION_NAME = 0x0AF48A3BE3BA84FA
HOLD_START = 65 / 60
EXTRA_SECONDS = 0.5


def decode(raw, start):
    at = start

    def take(fmt):
        nonlocal at
        result = struct.unpack_from('<' + fmt, raw, at)
        at += struct.calcsize('<' + fmt)
        return result[0] if len(result) == 1 else result

    _, count, length, _, headers, beats = take('IIfIII')
    if not 0 < count <= 512 or headers or beats:
        raise ValueError('Unsupported animation track or event layout')
    at += 2
    flag_bytes = math.ceil(count * 3 / 8)
    flag_bytes += flag_bytes % 2
    flags = raw[at:at + flag_bytes]
    at += flag_bytes

    def flag(index):
        return bool(flags[index // 8] & (1 << (index % 8)))

    def position(values):
        return [(x - 32767) * 10 / 32767 for x in values]

    def rotation(value):
        largest = value & 3
        x, y, z = [(((value >> shift) & 1023) - 512) * .75 / 512 for shift in (2, 12, 22)]
        last = math.sqrt(max(0, 1 - x*x - y*y - z*z))
        return ([last, x, y, z], [z, last, x, y], [y, z, last, x], [x, y, z, last])[largest]

    def scale(values):
        return [1 + x if not math.isnan(x) else 1 for x in values]

    tracks = []
    for i in range(count):
        pos = position(take('3H')) if flag(i*3) else list(take('3f'))
        rot = rotation(take('I')) if flag(i*3+1) else list(take('4f'))
        scl = scale(take('3e')) if flag(i*3+2) else [1 if math.isnan(x) else x for x in take('3f')]
        tracks.append(dict(positions=[[0, pos]], rotations=[[0, rot]], scales=[[0, scl]]))
    prefix_end = at
    entries = []
    while True:
        begin = at
        kind = take('H')
        typ = kind >> 14
        if not typ:
            if kind == 3:
                break
            bone, stamp = take('If')
            if kind == 2:
                entries.append((stamp, begin, at, False))
                continue
            if kind not in (4, 5, 6):
                raise ValueError('Unsupported uncompressed animation entry')
            values = list(take('4f' if kind == 5 else '3f'))
            field = {4: 'positions', 5: 'rotations', 6: 'scales'}[kind]
        else:
            stamp = (((kind & 15) << 16) | take('H')) / 1000
            bone = (kind & 0x3ff0) >> 4
            if typ == 1:
                values, field = scale(take('3e')), 'scales'
            elif typ == 2:
                values, field = position(take('3H')), 'positions'
            else:
                values, field = rotation(take('I')), 'rotations'
        if bone >= count:
            raise ValueError('Animation bone index out of range')
        tracks[bone][field].append([stamp, values])
        entries.append((stamp, begin, at, bool(typ)))
    footer = take('I')
    if footer != at - start - 4:
        raise ValueError('Animation footer does not match stream extent')
    return dict(end=at, length=length, tracks=tracks, prefix_end=prefix_end, entries=entries)


def interpolate(points, time, quaternion=False):
    points = sorted({float(t): list(v) for t, v in points}.items())
    if time <= points[0][0]:
        result = points[0][1]
    elif time >= points[-1][0]:
        result = points[-1][1]
    else:
        for (a, x), (b, y) in zip(points, points[1:]):
            if a <= time <= b:
                if quaternion and sum(i*j for i, j in zip(x, y)) < 0:
                    y = [-value for value in y]
                result = [i + (j-i) * (time-a) / (b-a) for i, j in zip(x, y)]
                break
    if quaternion:
        norm = math.sqrt(sum(x*x for x in result))
        result = [x/norm for x in result]
    return result


def add_hold(raw):
    parts, at = [], 0
    while at < len(raw):
        old = decode(raw, at)
        prefix = bytearray(raw[at:old['prefix_end']])
        struct.pack_into('<f', prefix, 8, old['length'] + EXTRA_SECONDS)
        entries = []
        for sequence, (stamp, begin, end, packed) in enumerate(old['entries']):
            entry = bytearray(raw[begin:end])
            shifted = stamp + EXTRA_SECONDS if stamp > HOLD_START else stamp
            if packed:
                kind = struct.unpack_from('<H', entry)[0]
                milliseconds = round(shifted * 1000)
                struct.pack_into('<HH', entry, 0, (kind & 0xfff0) | (milliseconds >> 16), milliseconds & 0xffff)
            else:
                struct.pack_into('<f', entry, 6, shifted)
            entries.append((shifted, sequence, bytes(entry)))
        sequence = len(entries)
        for bone, track in enumerate(old['tracks']):
            for field, kind in (('positions', 4), ('rotations', 5), ('scales', 6)):
                if len(track[field]) == 1:
                    continue
                values = interpolate(track[field], HOLD_START, field == 'rotations')
                for stamp in (HOLD_START, HOLD_START + EXTRA_SECONDS):
                    entry = struct.pack('<HIf', kind, bone, stamp) + struct.pack('<' + 'f'*len(values), *values)
                    entries.append((stamp, sequence, entry))
                    sequence += 1
        entries.sort(key=lambda x: (x[0], x[1]))
        body = prefix + b''.join(x[2] for x in entries) + struct.pack('<H', 3)
        body += struct.pack('<I', len(body))
        parts.append(body)
        at = old['end']
    if len(parts) != 2:
        raise ValueError('Expected two animation streams')
    result = bytearray(b''.join(parts))
    struct.pack_into('<I', result, 12, len(result))
    struct.pack_into('<I', result, len(parts[0]) + 12, 0)
    return bytes(result)


def extract_input(path):
    raw = path.read_bytes()
    if struct.unpack_from('<III', raw) != (0xf0000011, 1, 1):
        raise ValueError('Expected one animation resource in the input archive')
    row = struct.unpack_from('<7Q6I', raw, 104)
    if row[:2] != (ANIMATION_NAME, ANIMATION_TYPE) or row[8] or row[9]:
        raise ValueError('Unexpected input resource identity or extra resource segments')
    resource = raw[row[2]:row[2]+row[7]]
    if len(resource) != row[7] or hashlib.sha256(resource).hexdigest() != SOURCE_SHA:
        raise ValueError('Updated game animation differs from the supported source')
    return resource


def archive_for(clip):
    body = bytearray(192) + clip
    body += bytes(-len(body) % 16)
    body[:72] = struct.pack('<III20sQQ24s', 0xf0000011, 1, 1, b'', len(body), 0, b'')
    body[72:104] = struct.pack('<IIQIIII', 0, 0, ANIMATION_TYPE, 1, 0, 16, 16)
    body[104:184] = struct.pack('<7Q6I', ANIMATION_NAME, ANIMATION_TYPE, 192, 0, 0, 0, 0, len(clip), 0, 0, 16, 16, 0)
    return bytes(body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install-root', type=Path, required=True)
    parser.add_argument('--game-data', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    install, data, output = args.install_root.resolve(), args.game_data.resolve(), args.out.resolve()
    mod = Path(__file__).resolve().parents[1]
    if output.exists():
        raise ValueError('Refusing to overwrite an existing output folder')
    if any(output == path or path in output.parents for path in (install, data, mod)):
        raise ValueError('Use a new output folder outside the installations and mod source')
    cli = install/'app/cli/HellForge.Cli.exe'
    if not cli.is_file() or not (data/'bundles.nxa').is_file():
        raise ValueError('HellForge CLI or updated game bundles.nxa is missing')
    if json.loads((install/'hellforge-install.json').read_text())['buildId'] != '25480438':
        raise ValueError('Unsupported HellForge game build')
    expected = json.loads((mod/'tools/expected-assets.json').read_text())
    if expected['schema'] != 1 or len(expected['files']) != 24:
        raise ValueError('Unexpected output asset inventory')
    output.mkdir(parents=True)
    probe = output/'input-spec'
    probe.mkdir()
    (probe/'hellforge-mod.json').write_text(json.dumps(dict(format=1, id='superearth.enemy-asset-input', name='Enemy animation input', version='0.0.0', gameplay=True)))
    spec = dict(format=1, patches=[dict(archives=['218cf6382ed2058e'], resources=[dict(type='animation', include=['0x0AF48A3BE3BA84FA'])])])
    (probe/'hellforge-data.json').write_text(json.dumps(spec))
    built = output/'input-stock'
    result = subprocess.run([str(cli), 'build-data', '--mod', str(probe), '--game-data', str(data), '--out', str(built)], capture_output=True, text=True)
    (output/'build-input.log').write_text(result.stdout+'\n'+result.stderr)
    if result.returncode:
        raise RuntimeError('Input extraction failed; see build-input.log')
    clip = add_hold(extract_input(built/'218cf6382ed2058e.patch_0'))
    if hashlib.sha256(clip).hexdigest() != OUTPUT_SHA:
        raise ValueError('Prepared animation failed its exact output checksum')
    archive = archive_for(clip)
    files = {name: b'' if name.endswith(('.gpu_resources', '.stream')) else archive for name in expected['files']}
    for name, raw in files.items():
        if Path(name).name != name or hashlib.sha256(raw).hexdigest().upper() != expected['files'][name]:
            raise ValueError('Prepared archive inventory or checksum mismatch')
    destination = output/'data'
    destination.mkdir()
    for name, raw in files.items():
        (destination/name).write_bytes(raw)
    report = dict(result='PASS', archives=8, resource_instances=8, extra_pause_seconds=EXTRA_SECONDS,
                  animation_sha256=OUTPUT_SHA, asset_files=expected['files'])
    (output/'asset-preparation-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Local animation preparation PASS: copy '+str(destination)+' into the Enemy Rebalance source folder.')


if __name__ == '__main__':
    main()
