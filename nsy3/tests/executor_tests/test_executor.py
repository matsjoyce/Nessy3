import pathlib
import pytest
import re

from nsy3 import execution

FILES = pathlib.Path(__file__).parent.glob("*.nsy3")

@pytest.mark.parametrize("file", [pytest.param(x, id=x.name) for x in FILES])
def test_file(file):
    out = execution.execute_file(file, return_stdout=True).decode()
    num_assertions = int(re.search(r"Assertions: (\d+)", out).group(1))
    assert num_assertions == out.count("Assertion passed")
