import subprocess
import pathlib

from . import compile, parser, serialisation


EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"

def compile_file(fname, search_paths=None):
    if search_paths is None:
        search_paths = [fname.parent]
    comp_fname = fname.with_suffix(".nsy3c")

    with fname.open() as f:
        a = parser.parse(f.read())

    rel_path = min((fname.resolve().relative_to(p.resolve()) for p in search_paths), key=lambda p: len(p.parts))
    if rel_path.stem == "__main__":
        rel_path = rel_path.parent
    modname = ".".join(rel_path.with_suffix("").parts)

    print(a.to_str())
    c = compile.compile(a, fname, modname)
    print(c.to_str())

    with comp_fname.open("wb") as f:
        f.write(c.to_bytes())

    return comp_fname


def read_compiled_header(fname):
    with fname.open("rb") as f:
        return serialisation.deserialise_from_file(f)


def find_module(modname, search_paths):
    for path in search_paths:
        basename = path / pathlib.Path(modname.replace(".", "/"))
        print(basename)
        if basename.with_suffix(".nsy3").exists():
            return basename.with_suffix(".nsy3")
        if (basename / "__main__.nsy3").exists():
            return (basename / "__main__.nsy3")
        # TODO folders as empty modules
    raise RuntimeError(f"Could not find {modname}")


def compile_files(fname, seen=None, search_paths=None):
    if search_paths is None:
        search_paths = [fname.parent]
    if seen is None:
        seen = set()
    files = []
    comp_fname = compile_file(fname, search_paths)
    header = read_compiled_header(comp_fname)
    for module in header["imports"]:
        if module not in seen:
            f = find_module(module, search_paths)
            seen.add(f)
            files.extend(compile_files(f, seen=seen, search_paths=search_paths))
    files.append(comp_fname)
    return files


def prepare_runspec(fname, search_paths=None):
    comp_files = compile_files(fname, search_paths=search_paths)
    runspec = {
        "files": [str(f) for f in comp_files]
    }
    return serialisation.serialise(runspec)


def execute_file(fname, return_stdout=False, search_paths=None):
    runspec = prepare_runspec(fname, search_paths=search_paths)
    proc = subprocess.Popen([EXECUTOR, "runspec", "-"], stdin=subprocess.PIPE, stdout=subprocess.PIPE if return_stdout else None)#, stderr=subprocess.PIPE)
    try:
        stdout, stderr = proc.communicate(runspec, timeout=10)
    except:
        proc.kill()
        stdout, stderr = proc.communicate()
    if proc.returncode:
        print(stderr, stdout)
        raise RuntimeError("Execution failed")
    if return_stdout:
        return stdout

