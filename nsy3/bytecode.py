import itertools
import struct


class BCodeMeta(type):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        id = 0
        for name, item in self.__dict__.items():
            if isinstance(item, BCodeType):
                item.name = name
                if item.emit:
                    item.id = id
                    id += 1


def indent(lst):
    return ["    " + x for x in lst]


class BCode:
    __slots__ = ["type", "arg_v", "subs_v", "pos", "tok"]
    BYTES_PATTERN = struct.Struct("<BI")

    def __init__(self, type, arg_v, subs_v, tok):
        self.type = type
        self.arg_v = arg_v
        self.subs_v = subs_v
        self.pos = None
        self.tok = tok

    def __repr__(self):
        return f"BCode({self.type}, {self.arg_v})"

    def __str__(self):
        if self.type.arg:
            return f"{self.type} {self.arg_v}"
        else:
            return f"{self.type}"

    def pprint(self):
        return [str(self)] + indent([z for x in self.subs_v for z in x.pprint()])

    def to_str(self):
        return "\n".join(self.pprint())

    def linearize(self):
        if not self.subs_v:
            me = self
        else:
            me = type(self)(self.type, self.arg_v, (), self.tok)
        return [z for x in self.subs_v for z in x.linearize()] + ([me] if self.type is not Bytecode.SEQ else [])

    def to_bytes(self):
        if self.arg_v:
            arg = self.arg_v.pos if isinstance(self.arg_v, BCode) else int(self.arg_v)
        else:
            arg = 0
        #print(self.type, arg)
        return [z for x in self.subs_v for z in x.to_bytes()] + ([self.BYTES_PATTERN.pack(self.type.id, arg)] if self.type.emit else [])

    def size(self):
        return 5

    def resolve_labels(self, start=0):
        #if self.type is Bytecode.LABEL:
            #print("L",self.arg_v)
        for sub in self.subs_v:
            start = sub.resolve_labels(start)
        self.pos = start
        return start + self.type.emit * self.size()


class BCodeType:
    def __init__(self, arg=None, subs=(), auto_arg=None, emit=True):
        self.arg = arg
        self.subs = subs
        self.auto_arg = auto_arg
        self.emit = emit
        self.id = self.name = None

    def __str__(self):
        return self.name

    def __call__(self, *args, tok=None):
        if self.arg:
            arg_v = args[0]
            subs_v = args[1:]
        elif self.auto_arg:
            arg_v = self.auto_arg(args)
            subs_v = args
        else:
            arg_v = None
            subs_v = args
        return BCode(self, arg_v, subs_v, tok)


class Seq:
    def __init__(self, ops):
        self.ops = ops


class Bytecode(metaclass=BCodeMeta):
    KWARG = BCodeType("name", ("value"))
    GETATTR = BCodeType(subs=("obj", "name"))
    CALL = BCodeType(subs=("func", "*args"), auto_arg=lambda s: len(s) - 1)
    BINOP = BCodeType("op", subs=("left", "right"))
    GET = BCodeType("name")
    SET = BCodeType("name", ("value"))
    CONST = BCodeType("value")
    JUMP = BCodeType("pos")
    JUMP_IF = BCodeType("pos", ("cond"))
    JUMP_IFNOT = BCodeType("pos", ("cond"))
    # No pop versions
    JUMP_IF_KEEP = BCodeType("pos", ("cond"))
    JUMP_IFNOT_KEEP = BCodeType("pos", ("cond"))
    DROP = BCodeType("num")
    RETURN = BCodeType(subs=("expr"))
    GETENV = BCodeType()
    # first half: pos is either a position to jump to, or 0 to indicate a thunk return
    # second half: number of stack elements to keep
    SETSKIP = BCodeType("pos")
    DUP = BCodeType("times")
    ROT = BCodeType("num")
    RROT = BCodeType("num")
    BUILDLIST = BCodeType("len", subs=("*items"))
    # first half: number of variables
    # second half: idx of * variable
    UNPACK = BCodeType("arg")
    # Add a variable that should be set to a thunk when a skip occurs, list is cleared by SETSKIP
    SKIPVAR = BCodeType("name")

    # Fake ops

    SEQ = BCodeType(subs=("ops"), emit=False)
    LABEL = BCodeType("id", emit=False)
    LINENO = BCodeType("lineno", emit=False)
    IGNORE = BCodeType(emit=False)


class Combine:
    def __init__(self, a, b):
        self.a, self.b = a, b

    def __str__(self):
        return f"{self.a} | {self.b}"

    def __index__(self):
        return ((self.a.pos if isinstance(self.a, BCode) else self.a) & 0xFFFF) + (self.b << 16)
