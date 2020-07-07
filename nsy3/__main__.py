import sys
import pathlib
import subprocess

from . import compile, parser

EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"

fname = pathlib.Path(sys.argv[1])
comp_fname = fname.with_suffix(".nsy3c")

with fname.open() as f:
    a = parser.parse(f.read())

c = compile.compile(a)
print(c.to_str())
with comp_fname.open("wb") as f:
    f.write(c.to_bytes())

proc = subprocess.run([EXECUTOR, comp_fname])
if proc.returncode:
    print("Err, executor returned", proc.returncode)
