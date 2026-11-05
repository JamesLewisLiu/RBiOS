"""Compare native gmframe draws with independent upstream AP2/GE2D parsing.

Optional analysis dependencies and original files are supplied explicitly. No
game execution, network access, decoded media files or asset rewriting occurs.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import types

REFERENCE_COMMIT = '1e66fcd780afb3088ce8e78c88615a143db492ee'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True, help='Analysis .tools directory')
    parser.add_argument('--executable', type=Path, required=True, help='Native rbafptimeline test executable')
    parser.add_argument('--output', type=Path, required=True, help='Analysis JSON report path')
    args = parser.parse_args()
    base = args.dependencies / 'afp-reference'
    for name, path in [('bemani', base / 'bemani'), ('bemani.format', base / 'bemani/format'),
                       ('bemani.format.afp', base / 'bemani/format/afp')]:
        module = types.ModuleType(name)
        module.__path__ = [str(path.resolve())]
        sys.modules[name] = module
    sys.path.insert(0, str(args.dependencies.resolve()))
    from bemani.format.afp.swf import SWF
    from bemani.format.afp.geo import Shape
    from bemani.format.afp.types import Matrix, Point
    from ifstools import IFS
    from kbinxml import KBinXML

    path = args.game_root / 'data/graphics_rb6/gmframe_00.ifs'
    archive = IFS(str(path), super_disable=True)
    try:
        files = {p.full_path.replace('\\', '/'): p for p in archive.tree.all_files}
        movie_bytes = files['afp/gmframe'].load(convert_kbin=False)
        movie = SWF('gmframe', movie_bytes, files['afp/bsi/gmframe'].load(convert_kbin=False))
        movie.parse()
        assert movie.exported_tags['gmframe'] == 245
        assert movie.exported_tags['frame_down'] == 238
        assert (movie.location.left, movie.location.right, movie.location.top, movie.location.bottom) == (0, 798, 0, 1360)
        definitions = {}

        def index(timeline):
            for tag in timeline.tags:
                if tag.id is not None:
                    definitions[tag.id] = tag
                if hasattr(tag, 'tags'):
                    index(tag)

        index(movie)
        assert definitions[245].labels == {'normal': 1, 'off': 2, 'on': 4}
        assert len(definitions[238].frames) == 112
        textures = {}
        xml = KBinXML(next(f for name, f in files.items() if 'texturelist' in name).load(convert_kbin=False)).xml_doc
        for sheet in xml:
            size = list(map(int, sheet.find('size').text.split()))
            for image in sheet.findall('image'):
                rect = list(map(int, image.find('uvrect').text.split()))
                textures[image.attrib['name']] = {
                    'size': ((rect[1] - rect[0]) / 2, (rect[3] - rect[2]) / 2),
                    'uv': [(rect[x] / (2 * size[0]), rect[y] / (2 * size[1]))
                           for x, y in [(0, 2), (1, 2), (0, 3), (1, 3)]]}
        shapes = {}
        hidden = {prefix + '_' + side for prefix in ['name', 'torina', 'shop', 'area', 'characard_s']
                  for side in ['you', 'rival']}
        masks_found = 0
        scripts_found = 0
        for symbol in definitions.values():
            for tag in getattr(symbol, 'tags', []):
                data = tag.as_dict()
                if data['type'] == 'AP2DoActionTag':
                    scripts_found += 1
                    objects = [value for action in data['bytecode']['actions'] for value in action.get('objects', [])]
                    assert objects in [
                        ['THIS', '1', 'StringConstant(0x390: aeplib)', 'StringConstant(0x440: stop)'],
                        ['StringConstant(0x20e: normal)', 'THIS', '2', 'StringConstant(0x390: aeplib)',
                         'StringConstant(0x442: gotoAndPlay)']], objects
                for event, programs in data.get('triggers', {}).items():
                    assert int(event) == 1 and len(programs) == 1
                    actions = programs[0]['actions']
                    objects = [value for action in actions for value in action.get('objects', [])]
                    assert objects == ['232', '0', '798', '0', 'THIS', '5', 'StringConstant(0x390: aeplib)',
                                       'StringConstant(0x833: aep_set_rect_mask)'], objects
                    assert [action['action'] for action in actions] == ['PUSH', 'GET_VARIABLE', 'PUSH', 'CALL_METHOD', 'POP', 'END']
                    masks_found += 1
        assert masks_found == 1 and scripts_found == 2

        def sample(symbol_id, frame, elapsed, parent, color, blend, instance_path, binding, clips, replacements):
            symbol = definitions[symbol_id]
            if hasattr(symbol, 'tags'):
                display = {}
                for current_frame in range(frame + 1):
                    spec = symbol.frames[current_frame]
                    for tag in symbol.tags[spec.start_tag_offset:spec.start_tag_offset + spec.num_tags]:
                        kind = type(tag).__name__
                        if kind == 'AP2DoActionTag':
                            continue  # Validated above; caller supplies the parent display-list frame.
                        assert kind == 'AP2PlaceObjectTag', kind
                        if tag.update:
                            old, born = display[tag.depth]
                            if tag.source_tag_id is not None:
                                old.source_tag_id = tag.source_tag_id
                            if tag.transform is not None:
                                old.transform = old.transform.update(tag.transform, False)
                            for attribute in ['rotation_origin', 'mult_color', 'add_color', 'blend']:
                                if getattr(tag, attribute) is not None:
                                    setattr(old, attribute, getattr(tag, attribute))
                        else:
                            display[tag.depth] = (copy.deepcopy(tag), current_frame)
                result = []
                for depth, (placement, born) in sorted(display.items()):
                    name = placement.movie_name or ''
                    if name in hidden:
                        continue
                    transform = (placement.transform or Matrix.identity()).multiply(parent).translate(
                        Point.identity().subtract(placement.rotation_origin or Point.identity()))
                    if placement.add_color is not None:
                        assert placement.add_color.as_tuple() == (0, 0, 0, 0)
                    mult = placement.mult_color
                    child_color = [color[i] * ([mult.r, mult.g, mult.b, mult.a][i] if mult else 1) for i in range(4)]
                    child_clips = list(clips)
                    if placement.as_dict().get('triggers'):
                        child_clips.append([coordinate for x, y in [(0, 232), (798, 232), (0, 0), (798, 0)]
                                            for coordinate in (transform.multiply_point(Point(x, y)).x,
                                                               transform.multiply_point(Point(x, y)).y)])
                    child = definitions[placement.source_tag_id]
                    age = elapsed - born
                    child_frame = age % len(child.frames) if hasattr(child, 'frames') else age
                    result.extend(sample(placement.source_tag_id, child_frame, age, transform, child_color,
                                         placement.blend or blend, instance_path + ('/' + name if name else ''),
                                         name or binding, child_clips, replacements))
                return result
            if color[3] == 0:
                return []
            if symbol_id not in shapes:
                shape = Shape(symbol.reference, files['geo/' + symbol.reference].load(convert_kbin=False))
                shape.parse()
                shapes[symbol_id] = shape
            shape = shapes[symbol_id]
            assert len(shape.draw_params) == 1 and len(shape.vertex_points) == 4
            draw = shape.draw_params[0]
            assert draw.flags == 3 and len(shape.tex_points) == 4
            assert draw.vertexes == [0, 1, 2, 2, 1, 3]
            for actual, expected in zip(shape.tex_points, textures[draw.region]['uv']):
                assert abs(actual.x - expected[0]) < 1e-7 and abs(actual.y - expected[1]) < 1e-7
            region = replacements.get(binding, draw.region)
            bounds = shape.bounds
            assert textures[region]['size'] == (bounds.right - bounds.left, bounds.bottom - bounds.top)
            return [{'texture': region, 'numbers': [parent.a, parent.b, parent.c, parent.d, parent.tx, parent.ty,
                                                    bounds.left, bounds.right, bounds.top, bounds.bottom, *color, blend],
                     'path': instance_path, 'clips': clips}]

        elapsed_values = [*range(224), 1000, 10000, 100000]
        rows, expected = [], []
        for difficulty in range(4):
            replacements = {'frame_round': 'frame_round_00', 'frame_dif': ['frame_dif_b', 'frame_dif_m', 'frame_dif_h', 'frame_dif_wh'][difficulty],
                            'player_you': 'player_you_red', 'player_rival': 'player_rival_blue',
                            'orb_you': 'orb_red', 'orb_rival': 'orb_blue'}
            for elapsed in elapsed_values:
                frame = min(elapsed, 3)
                rows.append(f'gmframe {frame} {elapsed} {difficulty}')
                expected.append(sample(245, frame, elapsed, Matrix.identity(), [1, 1, 1, 1], 0, 'gmframe', '', [], replacements))
        process = subprocess.run([str(args.executable.resolve()), '--frame-sample', str(path.resolve())],
                                 input='\n'.join(rows) + '\n', capture_output=True, text=True, check=True)
        lines = process.stdout.splitlines()
        assert len(lines) == len(rows), (len(lines), len(rows), process.stderr)
        commands = 0
        masked = 0
        timer_textures = set()
        for row, line, wanted in zip(rows, lines, expected):
            parts = iter(line.split())
            assert int(next(parts)) == len(wanted), (row, line[:100], len(wanted))
            for draw in wanted:
                texture = next(parts)
                assert texture == draw['texture'], (row, texture, draw['texture'])
                for value in draw['numbers']:
                    actual = float(next(parts))
                    assert abs(actual - value) < 0.0002, (row, texture, actual, value)
                actual_path = next(parts)
                assert actual_path == draw['path'], (row, actual_path, draw['path'])
                clip_count = int(next(parts))
                assert clip_count == len(draw['clips']), (row, clip_count, draw['clips'])
                for clip in draw['clips']:
                    for value in clip:
                        actual = float(next(parts))
                        assert abs(actual - value) < 0.0002, (row, texture, 'mask', actual, value)
                commands += 1
                masked += bool(clip_count)
                if texture.startswith('timer_eff_'):
                    timer_textures.add(texture)
            assert next(parts, None) is None, row
        assert len(timer_textures) == 56 and masked > 0
        report = {'status': 'passed', 'reference_commit': REFERENCE_COMMIT,
                  'package_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                  'movie_sha256': hashlib.sha256(movie_bytes).hexdigest(),
                  'difficulties': 4, 'samples': len(rows), 'commands': commands, 'masked_commands': masked,
                  'timer_textures': len(timer_textures), 'elapsed_ticks_per_difficulty': elapsed_values,
                  'scope': 'normal frame with original named texture replacements, hidden unavailable account bindings, independent child clocks and original rectangular mask',
                  'limits': ['not an original Windows render trace', 'not full global aeplib execution',
                             'no original account text, portraits, score widgets or unimplemented page logic',
                             'not device pixels or full original visual equivalence']}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(f'PASS {len(rows)} frame samples, {commands} draws, {masked} masked draws, {len(timer_textures)} timer textures')
    finally:
        archive.close()


if __name__ == '__main__':
    main()
