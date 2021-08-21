"""
Usage:
    nsy3 single <fname> [--noexec] [--runspec=<rsfile>]
    nsy3 full <dir> [--noexec] [--runspec=<rsfile>]
"""

import pathlib
import subprocess
import docopt

from . import execution

EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"

args = docopt.docopt(__doc__)

if args["single"]:
    f = pathlib.Path(args["<fname>"]).resolve()
    runspec = execution.Runspec([f.parent])
    runspec.add_fname(f)
else:
    f = pathlib.Path(args["<dir>"]).resolve()
    runspec = execution.Runspec([f])
    for fname in f.glob("**/*.nsy3"):
        runspec.add_fname(fname)

if args["--noexec"]:
    pass
elif args["--runspec"]:
    with open(args["--runspec"], "wb") as f:
        f.write(runspec.to_bytes())
else:
    print("Running executor on", runspec)
    print(runspec.execute(return_dvs=True))
