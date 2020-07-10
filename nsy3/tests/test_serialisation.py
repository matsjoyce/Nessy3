from nsy3 import serialisation


def test_simple():
    cases = {
        1: b"\x00\x01\x00\x00\x00",
        2.5: b"\x01\x00\x00\x00\x00\x00\x00\x04@",
        "hello": b"\x02\x05\x00\x00\x00hello"
    }
    for obj, ser in cases.items():
        assert serialisation.serialise(obj) == ser

    for obj, ser in cases.items():
        assert serialisation.deserialise(ser) == (obj, len(ser))


def test_seqs():
    # Since sets aren't ordered consistently, don't put strings in the test sets because they may move around from execution to execution
    cases = [
        ([1, 2, 3, "blue"], b"\x05\x04\x00\x00\x00\x00\x01\x00\x00\x00\x00\x02\x00\x00\x00\x00\x03\x00\x00\x00\x02\x04\x00\x00\x00blue"),
        ({1, 2, 3.5}, b"\x04\x03\x00\x00\x00\x00\x01\x00\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x0c@"),
        ({1: 2.5, 3.5: "x", "x": 2}, b"\x03\x03\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x04@\x01\x00\x00\x00\x00\x00\x00\x0c@\x02\x01\x00\x00\x00x\x02\x01\x00\x00\x00x\x00\x02\x00\x00\x00")
    ]
    for obj, ser in cases:
        assert serialisation.serialise(obj) == ser

    for obj, ser in cases:
        assert serialisation.deserialise(ser) == (obj, len(ser))
