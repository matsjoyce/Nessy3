"""
Usage:
    nsy3 <fname> [--noexec] [--runspec=<rsfile>]
"""

import pathlib
import subprocess
import docopt

from . import execution

EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"

args = docopt.docopt(__doc__)

fname = pathlib.Path(args["<fname>"])

if args["--noexec"]:
    execution.compile_file(fname)
elif args["--runspec"]:
    with open(args["--runspec"], "wb") as f:
        f.write(execution.prepare_runspec(fname))
else:
    print("Running executor...")
    execution.execute_file(fname)
