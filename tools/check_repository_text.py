"""Check authored repository text for valid UTF-8 and Han-character regressions.

Original game assets are not repository inputs. Non-ASCII encoding fixtures can
use source Unicode escapes without changing their runtime values.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {
    '.md', '.txt', '.py', '.swift', '.mm', '.cpp', '.hpp', '.h', '.json',
    '.yml', '.yaml', '.plist', '.pbxproj', '.xcscheme', '.sh', '.cmake',
}
TEXT_NAMES = {'CMakeLists.txt', '.gitignore', '.gitattributes', '.clang-format'}
HAN = re.compile('[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\U00020000-\U000323af]')


def main():
    names = subprocess.check_output(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT
    ).decode('utf-8').split('\0')
    errors = []
    checked = 0
    for name in sorted(set(filter(None, names))):
        path = ROOT / name
        if path.suffix not in TEXT_SUFFIXES and path.name not in TEXT_NAMES:
            continue
        try:
            source = path.read_text(encoding='utf-8-sig')
        except UnicodeError:
            errors.append(f'{ascii(name)}: invalid UTF-8')
            continue
        checked += 1
        for line, value in enumerate(source.splitlines(), 1):
            if HAN.search(value):
                errors.append(f'{ascii(name)}:{line}: literal Han text; use English prose or escaped fixtures')
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'PASS repository text: {checked} UTF-8 files; no literal Han characters')


if __name__ == '__main__':
    main()
