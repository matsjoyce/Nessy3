"""
Usage:
    nsy3 <fname> [--noexec]
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
else:
    print("Running executor...")
    execution.execute_file(fname)
