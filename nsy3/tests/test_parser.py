import pathlib

from nsy3 import parser


#def test_parser():
    #with pathlib.Path(__file__).with_suffix(".nsy3").open() as f:
        #parser.parse(f.read()).to_str()


def test_parser_empty():
    parser.parse("")
    parser.parse("\n")
    parser.parse("\n\n")
    parser.parse("\n \n\n   \n\n ")

def test_prec():
    assert parser.parse(r"\\x -> \\y -> x + y") == parser.parse(r"\\x -> (\\y -> (x + y))")
    assert parser.parse(r"1 if 2 else 4 if 5 else 6") == parser.parse(r"1 if 2 else (4 if 5 else 6)")
    assert parser.parse(r"1 if 2 if 4 else 5 else 6") == parser.parse(r"1 if (2 if 4 else 5) else 6")
    assert parser.parse(r"\\x -> x if y else 2") == parser.parse(r"\\x -> (x if y else 2)")
