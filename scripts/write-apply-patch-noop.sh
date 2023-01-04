#!/bin/bash
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE:-$0}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"

cat >"$BASE_PATH/src/electron/script/apply_all_patches.py" <<EOF
#!/usr/bin/env python3

def apply_patches(dirs):
  print("apply_patches - no-op")

def parse_args():
  return {}

def main():
  print("apply_patches - no-op")

if __name__ == '__main__':
  main()
EOF
