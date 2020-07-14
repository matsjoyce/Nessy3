import dataclasses


def indent(stuff):
    if all(indent == stuff[0][0] for indent, _ in stuff[1:]):
        return [(stuff[0][0] + 1, " ".join(text for _, text in stuff))]
    return [(indent + 1, text) for indent, text in stuff]


@dataclasses.dataclass(repr=False)
class ASTNode:
    tok: object

    def __post_init__(self):
        try:
            self.lineno = self.tok.lineno
        except AttributeError:
            self.lineno = None

    def to_str(self, prefix=""):
        return "\n".join(prefix + "  " * indent + text for indent, text in self.pprint())


class Expr(ASTNode):
    pass


@dataclasses.dataclass(repr=False)
class Func(Expr):
    args: list
    stmt: ASTNode

    def pprint(self):
        args = []
        for arg in self.args:
            if args:
                args.append((0, ","))
            args.append((0, arg[0]))
            if arg[1] is not None:
                args.append((0, "="))
                args.extend(arg[1].pprint())
        return [(0, f"\\\\")] + indent(args + [(0, "->")] + self.stmt.pprint())


@dataclasses.dataclass(repr=False)
class Monop(Expr):
    op: str
    value: Expr

    def pprint(self):
        return indent([(0, self.op), (0, "(")] + self.value.pprint() + [(0, ")")])


@dataclasses.dataclass(repr=False)
class Binop(Expr):
    op: str
    left: Expr
    right: Expr

    def pprint(self):
        return indent([(0, self.op), (0, "(")] + self.left.pprint() + [(0, ",")] + self.right.pprint() + [(0, ")")])


@dataclasses.dataclass(repr=False)
class Getattr(Expr):
    left: Expr
    right: str

    def pprint(self):
        return self.left.pprint() + [(0, "." + self.right)]


@dataclasses.dataclass(repr=False)
class Call(Expr):
    func: ASTNode
    args: list # of ASTNode

    def pprint(self):
        args, kwargs = [], []
        for arg in self.args:
            if args:
                args.append((0, ","))
            if isinstance(arg, tuple):
                args.extend(([0, arg[0]], [0, "="]))
                args.extend(arg[1].pprint())
            else:
                args.extend(arg.pprint())
        return indent([(0, "(")] + self.func.pprint() + [(0, ")")] + [(0, "(")] + args + kwargs + [(0, ")")])


@dataclasses.dataclass(repr=False)
class Literal(Expr):
    value: object

    def pprint(self):
        return [(0, repr(self.value))]


@dataclasses.dataclass(repr=False)
class SequenceLiteral(Expr):
    type: str # one of [], {}, {:}
    seq: list # of Expr

    def pprint(self):
        seq = []
        for s in self.seq:
            if seq:
                seq.append((0, ","))
            if isinstance(s, tuple):
                seq.extend(s[0].pprint())
                seq.append((0, ":"))
                seq.extend(s[1].pprint())
            else:
                seq.extend(s.pprint())
        return [(0, f"{self.type}: [")] + seq + [(0, "]")]


@dataclasses.dataclass(repr=False)
class Name(Expr):
    name: str

    def pprint(self):
        return [(0, self.name)]


class Stmt(ASTNode):
    pass


@dataclasses.dataclass(repr=False)
class Pass(Stmt):
    def pprint(self):
        return [(0, "pass")]


@dataclasses.dataclass(repr=False)
class Break(Stmt):
    def pprint(self):
        return [(0, "break")]


@dataclasses.dataclass(repr=False)
class Continue(Stmt):
    def pprint(self):
        return [(0, "continue")]


@dataclasses.dataclass(repr=False)
class Return(Stmt):
    expr: Expr

    def pprint(self):
        return [(0, "return")] + self.expr.pprint()


@dataclasses.dataclass(repr=False)
class Assert(Stmt):
    expr: Expr

    def pprint(self):
        return [(0, "assert")] + self.expr.pprint()


@dataclasses.dataclass(repr=False)
class Block(ASTNode):
    stmts: list

    def pprint(self):
        return [(0, f"block")] + [l for stmt in self.stmts for l in indent(stmt.pprint())]


@dataclasses.dataclass(repr=False)
class ExprStmt(Stmt):
    expr: Expr

    def pprint(self):
        return self.expr.pprint()


@dataclasses.dataclass(repr=False)
class AssignStmt(Stmt):
    name: str
    expr: Expr

    def pprint(self):
        return [(0, self.name + " =")] + self.expr.pprint()


@dataclasses.dataclass(repr=False)
class IfStmt(Stmt):
    expr: Expr
    if_block: Block
    else_block: Block

    def pprint(self):
        return [(0, "if")] + indent(self.expr.pprint()) + [(0, "then")] + indent(self.if_block.pprint()) + [(0, "else")] + indent(self.else_block.pprint())


@dataclasses.dataclass(repr=False)
class WhileStmt(Stmt):
    expr: Expr
    block: Block

    def pprint(self):
        return [(0, "while")] + indent(self.expr.pprint()) + [(0, "do")] + indent(self.block.pprint())


@dataclasses.dataclass(repr=False)
class ForStmt(Stmt):
    name: str
    expr: Expr
    block: Block

    def pprint(self):
        return [(0, "for " + self.name + " in")] + indent(self.expr.pprint()) + [(0, "do")] + indent(self.block.pprint())


@dataclasses.dataclass(repr=False)
class ImportStmt(Stmt):
    modname: str
    names: list

    def pprint(self):
        return [(0, "from " + str(self.modname) + " import " + (", ".join(f"{a} as {b}" for a, b in self.names) if self.names else "_"))]
