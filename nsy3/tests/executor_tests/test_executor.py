import pathlib
import pytest

from nsy3 import execution

FILES = pathlib.Path(__file__).parent.glob("*.nsy3")

@pytest.mark.parametrize("file", [pytest.param(x, id=x.name) for x in FILES])
def test_file(file):
    execution.execute_file(file)
