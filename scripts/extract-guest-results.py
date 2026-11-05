#!/usr/bin/env python3
import re
import sys
from pathlib import Path


if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} QEMU_LOG OUTPUT_DIR")
source, output = Path(sys.argv[1]), Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
current = None
files = 0
for raw in source.read_text(errors="replace").splitlines():
    line = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", raw).rstrip("\r")
    if line.startswith("VACCEL_ARTIFACT_BEGIN "):
        current = (output / Path(line.split(" ", 1)[1]).name).open("w")
    elif line.startswith("VACCEL_ARTIFACT_END "):
        if current:
            current.close()
            files += 1
            current = None
    elif current:
        current.write(line + "\n")
if current:
    current.close()
    raise SystemExit("unterminated artifact in serial log")
print(f"extracted {files} artifacts to {output}")
