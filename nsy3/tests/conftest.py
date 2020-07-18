import pathlib
import subprocess

BUILD_DIR = pathlib.Path(__file__).parent.parent / "executor/build"

def pytest_configure(config):
    print("Removing old coverage files...", end=" ", flush=True)
    for path in BUILD_DIR.glob("**/*.gcda"):
        path.unlink()

    from nsy3 import execution

    execution.EXECUTOR = execution.EXECUTOR.with_name("executor_coverage")


def pytest_unconfigure(config):
    print("Generating C++ coverage report...", end=" ", flush=True)
    subprocess.check_call("lcov --capture --directory . --output-file .coverage.info -b . --include */nsy3/executor/*".split(),
                          stdout=subprocess.DEVNULL, cwd=BUILD_DIR)
    subprocess.check_call(["genhtml", BUILD_DIR / ".coverage.info", "--output-directory", "htmlcovpp", "--branch-coverage", "--demangle-cpp"],
                          stdout=subprocess.DEVNULL)
    pathlib.Path(BUILD_DIR / ".coverage.info").unlink()
    print("done")
