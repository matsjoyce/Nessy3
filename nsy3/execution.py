import subprocess
import pathlib

from . import compile, parser


EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"

def compile_file(fname):
    comp_fname = fname.with_suffix(".nsy3c")

    with fname.open() as f:
        a = parser.parse(f.read())

    c = compile.compile(a, fname)
    print(c.to_str())

    with comp_fname.open("wb") as f:
        f.write(c.to_bytes())

def execute_file(fname, return_stdout=False):
    comp_fname = fname.with_suffix(".nsy3c")
    compile_file(fname)
    kwargs = {}
    if return_stdout:
        kwargs["stdout"] = subprocess.PIPE
        kwargs["stderr"] = subprocess.STDOUT

    proc = subprocess.run([EXECUTOR, comp_fname], **kwargs)
    if proc.returncode:
        raise RuntimeError("Execution failed")
    if return_stdout:
        return proc.stdout

