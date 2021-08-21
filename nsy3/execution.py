import subprocess
import pathlib
import re

from . import compile, parser, serialisation


EXECUTOR = pathlib.Path(__file__).parent / "executor/build/executor"


class Runspec:
    def __init__(self, search_paths):
        self.search_paths = search_paths
        self.files = []
        self.compiled_files = []
        self.modules = []
        self.conclusion = None

    def add_fname(self, fname):
        if fname not in self.files:
            self.files.append(fname)
            comp_fname, modname = self.compile_file(fname)
            header = self.read_compiled_header(comp_fname)
            print(fname, header["imports"])
            for module in header["imports"]:
                self.add_fname(self.find_module(module))
            self.compiled_files.append(comp_fname)
            self.modules.append(modname)
        return self

    def compile_file(self, fname):
        comp_fname = fname.with_suffix(".nsy3c")

        with fname.open() as f:
            a = parser.parse(f.read())

        rel_path = min((fname.resolve().relative_to(p.resolve()) for p in self.search_paths), key=lambda p: len(p.parts))
        if rel_path.stem == "__main__":
            rel_path = rel_path.parent
        modname = ".".join(rel_path.with_suffix("").parts)

        #print(a.to_str())
        c = compile.compile(a, fname, modname)
        #print(c.to_str())

        with comp_fname.open("wb") as f:
            f.write(c.to_bytes())

        return comp_fname, modname

    @staticmethod
    def read_compiled_header(fname):
        with fname.open("rb") as f:
            return serialisation.deserialise_from_file(f)

    def find_module(self, modname):
        for path in self.search_paths:
            basename = path / pathlib.Path(modname.replace(".", "/"))
            print(basename)
            if basename.with_suffix(".nsy3").exists():
                return basename.with_suffix(".nsy3")
            if (basename / "__main__.nsy3").exists():
                return (basename / "__main__.nsy3")
            # TODO folders as empty modules
        raise RuntimeError(f"Could not find {modname}")

    def set_conclusion(self, code):
        if not code:
            self.conclusion = None
            return

        a = parser.parse(code)
        c = compile.compile(a, None, "conclusion")
        self.conclusion = c.to_bytes()


    def to_bytes(self):
        runspec = {
            "files": [str(f) for f in self.compiled_files],
            "modules": self.modules,
            "conclusion": self.conclusion
        }
        return serialisation.serialise(runspec)

    def __str__(self):
        return f"Runspec({self.search_paths}, compiled_files={self.compiled_files}, modules={self.modules})"

    def execute(self, return_stdout=False, return_dvs=False):
        proc = subprocess.Popen([EXECUTOR, "runspec", "-"], stdin=subprocess.PIPE, stdout=subprocess.PIPE if return_stdout or return_dvs else None)#, stderr=subprocess.PIPE)
        try:
            stdout, stderr = proc.communicate(self.to_bytes(), timeout=10)
        except:
            proc.kill()
            stdout, stderr = proc.communicate()
        if proc.returncode:
            print(stderr, stdout)
            raise RuntimeError("Execution failed")
        if return_stdout:
            return stdout
        if return_dvs:
            bytes = re.search(b"=== MARKER ===\n(.*)=== END MARKER ===", stdout, re.DOTALL).group(1)
            obj, pos = serialisation.deserialise(bytes)
            assert pos == len(bytes)
            return obj
