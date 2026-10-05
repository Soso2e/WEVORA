"""Launch the Mac Editor at Medium quality without editing project/Shipping config."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ue-root', default=os.environ.get('UE_ROOT'))
    parser.add_argument('--quality', choices=('low', 'medium'), default='medium')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if not args.ue_root:
        parser.error('Set UE_ROOT or pass --ue-root (UE 5.8 installation directory)')
    engine = Path(args.ue_root).expanduser().resolve()
    editor = engine / 'Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
    if not args.dry_run and (sys.platform != 'darwin' or not editor.is_file()):
        parser.error('This profile requires macOS and an installed UE 5.8 Mac Editor')
    project = Path(__file__).resolve().parents[1] / 'WEVORA.uproject'
    quality = 0 if args.quality == 'low' else 1
    groups = ('ViewDistance', 'AntiAliasing', 'Shadow', 'GlobalIllumination',
              'Reflection', 'PostProcess', 'Texture', 'Effects', 'Foliage', 'Shading')
    cvars = [f'sg.{group}Quality={quality}' for group in groups]
    cvars += ['r.ScreenPercentage=70', 'r.Streaming.PoolSize=512']
    # Process arguments only. No -game, config writes, or asset modifications.
    command = [str(editor), str(project), '-NoSplash',
               '-ExecCmds=' + ','.join(cvars)]
    if args.dry_run:
        print('\n'.join(command))
        return 0
    return subprocess.run(command, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
