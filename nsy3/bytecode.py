import itertools
import struct


class BCodeMeta(type):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.__ann_idxes__ = dict(zip(self.__annotations__.keys(), itertools.count()))

    def __getattr__(self, name):
        if name in self.__annotations__:
            return BCode(self.__ann_idxes__[name], self.__annotations__[name])
        raise AttributeError(name)

    def __getitem__(self, idx):
        return list(self.__annotations__.keys())[idx]


def indent(lst):
    return ["    " + x for x in lst]


class BCode:
    __slots__ = ["id", "bcodetype", "arg_v", "subs_v"]
    BYTES_PATTERN = struct.Struct("<BI")

    def __init__(self, id, bcodetype, arg_v=None, subs_v=None):
        self.id = id
        self.bcodetype = bcodetype
        self.arg_v = arg_v
        self.subs_v = subs_v

    def __call__(self, *args):
        if self.bcodetype.arg:
            arg = args[0]
            subs = args[1:]
        else:
            arg = None
            subs = args
        return type(self)(self.id, self.bcodetype, arg, subs)

    def __repr__(self):
        return f"BCode({self.id}, {self.arg_v})"

    def __str__(self):
        if self.bcodetype.arg:
            return f"{Bytecode[self.id]} {self.arg_v}"
        else:
            return f"{Bytecode[self.id]}"

    def pprint(self):
        return [str(self)] + indent([z for x in self.subs_v for z in x.pprint()])

    def to_str(self):
        return "\n".join(self.pprint())

    def to_bytes(self):
        if self.arg_v:
            arg = self.arg_v.pos if isinstance(self.arg_v, Label) else self.arg_v
        elif self.bcodetype.auto_arg:
            arg = self.bcodetype.auto_arg(self.subs_v)
        else:
            arg = 0
        print("X", self.id, arg)
        return [z for x in self.subs_v for z in x.to_bytes()] + ([self.BYTES_PATTERN.pack(self.id, arg)] if self.bcodetype.emit else [])

    def resolve_labels(self, start=0):
        for sub in self.subs_v:
            start = sub.resolve_labels(start)
        return start + self.bcodetype.emit


class BCodeType:
    def __init__(self, arg=None, subs=(), auto_arg=None, emit=True):
        self.arg = arg
        self.subs = subs
        self.auto_arg = auto_arg
        self.emit = emit


class Label:
    def __init__(self, id):
        self.id = id
        self.pos = None

    def pprint(self):
        return [str(self)]

    def __str__(self):
        return f"Label {self.id}"

    def to_bytes(self):
        return []

    def resolve_labels(self, start):
        self.pos = start
        return start


class Seq:
    def __init__(self, ops):
        self.ops = ops


class Bytecode(metaclass=BCodeMeta):
    KWARG: BCodeType("name", ("value"))
    GETATTR: BCodeType(subs=("obj", "name"))
    CALL: BCodeType(subs=("func", "args"), auto_arg=lambda s: len(s) - 1)
    GET: BCodeType("name")
    SET: BCodeType("name", ("value"))
    CONST: BCodeType("value")
    JUMP: BCodeType("pos")
    JUMP_NOTIF: BCodeType("pos", ("cond"))
    DROP: BCodeType("num")
    RETURN: BCodeType(subs=("expr"))
    GETENV: BCodeType()

    # Fake ops

    SEQ: BCodeType(subs=("ops"), emit=False)
