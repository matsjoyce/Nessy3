import itertools
import struct

from . import ast, bytecode, serialisation
from .bytecode import Bytecode


class ConversionContext:
    def __init__(self):
        self.counter = itertools.count()
        self.variables = {}
        self.globals = {}
        self.label_counter = itertools.count()
        self.loop_stack = []
        self.consts = []
        self.functions = []

    def lookup_var(self, name):
        if name not in self.variables:
            self.variables[name] = self.globals[name] = next(self.counter)
        return ir.Name(self.variables[name])

    def assign_var(self, name):
        n = self.variables[name] = next(self.counter)
        return n

    def label(self):
        return bytecode.Label(next(self.label_counter))

    def push_loop(self, *labels):
        self.loop_stack.append(labels)

    def pop_loop(self):
        return self.loop_stack.pop()

    def current_loop(self):
        return self.loop_stack[-1] if self.loop_stack else None

    def const(self, const, wrap=True):
        try:
            idx = self.consts.index(const)
        except ValueError:
            self.consts.append(const)
            idx = len(self.consts) - 1
        if not wrap:
            return idx
        return Bytecode.CONST(idx)

    def add_function(self, code):
        label = self.label()
        self.functions.append(Bytecode.SEQ(label, code))
        return label


def compile_expr(a, ctx):
    if isinstance(a, ast.Binop):
        if a.op in ["and", "or"]:
            # Short circuiting
            return ...
        return Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.left, ctx), ctx.const(a.op)), compile_expr(a.right, ctx))
    elif isinstance(a, ast.Monop):
        return Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.value, ctx), ctx.const("unary_" + a.op)))
    elif isinstance(a, ast.Call):
        args = []
        for arg in a.args:
            if isinstance(arg, tuple):
                args.append(Bytecode.KWARG(ctx.const(arg[0], wrap=False), compile_expr(arg[1], ctx)))
            else:
                args.append(compile_expr(arg, ctx))
        return Bytecode.CALL(compile_expr(a.func, ctx), *args)
    elif isinstance(a, ast.Literal):
        return ctx.const(a.value)
    elif isinstance(a, ast.SequenceLiteral):
        return Bytecode.CALL(Bytecode.GET(ctx.const(a.type, wrap=False)), *[compile_expr(s, ctx) for s in a.seq])
    elif isinstance(a, ast.Name):
        return Bytecode.GET(ctx.const(a.name, wrap=False))
    elif isinstance(a, ast.Func):
        func_label = ctx.add_function(Bytecode.SEQ(*list(compile_stmt(a.stmt, ctx))))
        return Bytecode.CALL(Bytecode.GET(ctx.const("->", wrap=False)), Bytecode.GET(ctx.const("__code__", wrap=False)), ctx.const(func_label), Bytecode.GETENV())
    else:
        raise RuntimeError(f"Cannot compile {type(a)} to IR.")
    return i


def compile_stmt(a, ctx):
    print(a)
    if isinstance(a, ast.Pass):
        return
    elif isinstance(a, ast.Break):
        labels = ctx.current_loop()
        if not labels:
            raise RuntimeError()
        yield Bytecode.JUMP(labels[1])
    elif isinstance(a, ast.Continue):
        labels = ctx.current_loop()
        if not labels:
            raise RuntimeError()
        yield Bytecode.JUMP(labels[0])
    elif isinstance(a, ast.Return):
        yield Bytecode.RETURN(compile_expr(a.expr, ctx))
    elif isinstance(a, ast.Block):
        for s in a.stmts:
            yield from compile_stmt(s, ctx)
    elif isinstance(a, ast.ExprStmt):
        yield compile_expr(a.expr, ctx)
        yield Bytecode.DROP(1)
    elif isinstance(a, ast.AssignStmt):
        yield Bytecode.SET(ctx.const(a.name, wrap=False), compile_expr(a.expr, ctx))
    elif isinstance(a, ast.IfStmt):
        else_label, end_label = ctx.label(), ctx.label()
        yield Bytecode.JUMP_NOTIF(else_label, compile_expr(a.expr, ctx))
        yield from compile_stmt(a.if_block, ctx)
        yield Bytecode.JUMP(end_label)
        yield else_label
        yield from compile_stmt(a.else_block, ctx)
        yield end_label
    elif isinstance(a, ast.WhileStmt):
        start_label, end_label = ctx.label(), ctx.label()
        ctx.push_loop(start_label, end_label)
        yield start_label
        yield Bytecode.JUMP_NOTIF(end_label, compile_expr(a.expr, ctx))
        yield from compile_stmt(a.block, ctx)
        yield Bytecode.JUMP(start_label)
        yield end_label
        ctx.pop_loop()
    elif isinstance(a, ast.ForStmt):
        return ...
    #elif isinstance(a, ast.):
    else:
        raise RuntimeError(f"Cannot compile {type(a)} to IR.")


class ConvertedCode:
    def __init__(self, code, ctx):
        self.code, self.ctx = code, ctx

    def to_bytes(self):
        full_code = Bytecode.SEQ(self.code, *self.ctx.functions)
        full_code.resolve_labels()
        consts = struct.pack("<I", len(self.ctx.consts)) + b"".join(serialisation.serialise(c.pos if isinstance(c, bytecode.Label) else c) for c in self.ctx.consts)
        bcode = b"".join(full_code.to_bytes())
        return consts + bcode

    def to_str(self):
        return f"CONSTS:\n    {self.ctx.consts}\n\n{self.code.to_str()}"


def compile(a):
    ctx = ConversionContext()
    i = Bytecode.SEQ(*list(compile_stmt(a, ctx)), Bytecode.RETURN(ctx.const(0)))
    return ConvertedCode(i, ctx)
