"""
Usage:
    nsy3 <fname> [--noexec]
"""

import pathlib
import subprocess
import docopt

from . import compile, parser

EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"

args = docopt.docopt(__doc__)

fname = pathlib.Path(args["<fname>"])
comp_fname = fname.with_suffix(".nsy3c")

with fname.open() as f:
    a = parser.parse(f.read())

c = compile.compile(a, fname)
print(c.to_str())

#sk = skipize.skipize(c)
#print(sk.to_str())

if args["--noexec"]:
    exit()

with comp_fname.open("wb") as f:
    f.write(c.to_bytes())

print("Running executor...")

proc = subprocess.run([EXECUTOR, comp_fname])
if proc.returncode:
    print("Err, executor returned", proc.returncode)
