import itertools
import struct

from . import ast, bytecode, serialisation
from .bytecode import Bytecode


class CompiledCode:
    def __init__(self, fname):
        self.fname = fname
        self.counter = itertools.count()
        self.variables = {}
        self.globals = {}
        self.label_counter = itertools.count()
        self.loop_stack = []
        self.consts = []
        self.functions = []
        self.code = None

    def lookup_var(self, name):
        if name not in self.variables:
            self.variables[name] = self.globals[name] = next(self.counter)
        return ir.Name(self.variables[name])

    def assign_var(self, name):
        n = self.variables[name] = next(self.counter)
        return n

    def label(self):
        return Bytecode.LABEL(next(self.label_counter))

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

    def create_linenotab(self, ops):
        lineno = pos = 0
        transitions = []
        for op in ops:
            if op.type == Bytecode.LINENO and op.arg_v != lineno:
                transitions.append((op.pos - pos, op.arg_v - lineno))
                lineno = op.arg_v
                pos = op.pos
        linenotab = bytearray()
        for num_bcode, num_lines in transitions:
            while num_bcode > 255:
                linenotab.append(255)
                linenotab.append(0)
                num_bcode -= 255
            while num_lines > 127:
                linenotab.append(0)
                linenotab.append(127)
                num_lines -= 127
            while num_lines < -128:
                linenotab.append(0)
                linenotab.append((-128) % 256)
                num_lines += 128
            linenotab.append(num_bcode)
            linenotab.append(num_lines % 256)
        print(linenotab)
        return bytes(linenotab)

    def to_bytes(self):
        full_code = Bytecode.SEQ(self.code, *self.functions)
        full_code.resolve_labels()
        linenotab = self.create_linenotab(full_code.linearize())
        full_code.resolve_labels()
        return serialisation.serialise({
            "consts": [(c.pos if isinstance(c, bytecode.BCode) else c) for c in self.consts],
            "fname": str(self.fname.resolve()),
            "linenotab": linenotab,
            "code": b"".join(full_code.to_bytes())
        })

    def to_str(self):
        stuff = ["Consts:", str(self.consts), "", "Code:", self.code.to_str()]
        for i, function in enumerate(self.functions):
            stuff.extend(["", f"F{i}:", function.to_str()])
        return "\n".join(stuff)


def compile_expr_iter(a, ctx):
    print(a)
    if a.lineno:
        yield Bytecode.LINENO(a.lineno)
    if isinstance(a, ast.Binop):
        if a.op in ["and", "or"]:
            # Short circuiting
            return ...
        elif a.op == ".":
            yield Bytecode.GETATTR(compile_expr(a.left, ctx), ctx.const(a.right))
            return
        yield Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.left, ctx), ctx.const(a.op)), compile_expr(a.right, ctx))
    elif isinstance(a, ast.Monop):
        yield Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.value, ctx), ctx.const("unary_" + a.op)))
    elif isinstance(a, ast.Call):
        args = []
        for arg in a.args:
            if isinstance(arg, tuple):
                args.append(Bytecode.KWARG(ctx.const(arg[0], wrap=False), compile_expr(arg[1], ctx)))
            else:
                args.append(compile_expr(arg, ctx))
        yield Bytecode.CALL(compile_expr(a.func, ctx), *args)
    elif isinstance(a, ast.Literal):
        yield ctx.const(a.value)
    elif isinstance(a, ast.SequenceLiteral):
        yield Bytecode.CALL(Bytecode.GET(ctx.const(a.type, wrap=False)), *[compile_expr(s, ctx) for s in a.seq])
    elif isinstance(a, ast.Name):
        yield Bytecode.GET(ctx.const(a.name, wrap=False))
    elif isinstance(a, ast.Func):
        func_label = ctx.add_function(Bytecode.SEQ(Bytecode.LINENO(a.lineno), *list(compile_stmt(a.stmt, ctx)), Bytecode.RETURN(ctx.const(0))))
        arg_names = []
        defaults = []
        for arg in a.args:
            arg_names.append(arg[0])
            if arg[1] is not None:
                defaults.append(compile_expr(arg[1], ctx))
        defaults_expr = Bytecode.CALL(Bytecode.GET(ctx.const("[]", wrap=False)), *defaults)
        signature = Bytecode.CALL(Bytecode.GET(ctx.const("Signature", wrap=False)), ctx.const(arg_names), defaults_expr, ctx.const(0))
        yield Bytecode.CALL(Bytecode.GET(ctx.const("->", wrap=False)), Bytecode.GET(ctx.const("__code__", wrap=False)), ctx.const(func_label), signature, Bytecode.GETENV())
    else:
        raise RuntimeError(f"Cannot compile {type(a)} to IR.")


def compile_expr(*args, **kwargs):
    instrs = list(compile_expr_iter(*args, **kwargs))
    if len(instrs) == 1:
        return instrs[0]
    return Bytecode.SEQ(*instrs)


def compile_stmt(a, ctx):
    if a.lineno:
        yield Bytecode.LINENO(a.lineno)
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
        yield Bytecode.SETSKIP(0)
        yield Bytecode.RETURN(compile_expr(a.expr, ctx))
    elif isinstance(a, ast.Block):
        for s in a.stmts:
            yield from compile_stmt(s, ctx)
    elif isinstance(a, ast.ExprStmt):
        skip_label = ctx.label()
        yield Bytecode.SETSKIP(skip_label)
        yield compile_expr(a.expr, ctx)
        yield Bytecode.DROP(1)
        yield skip_label
    elif isinstance(a, ast.AssignStmt):
        skip_label = ctx.label()
        yield Bytecode.SETSKIP(skip_label)
        yield Bytecode.SET(ctx.const(a.name, wrap=False), compile_expr(a.expr, ctx))
        yield skip_label
    elif isinstance(a, ast.IfStmt):
        else_label, end_label = ctx.label(), ctx.label()
        if_comp = Bytecode.SEQ(*list(compile_stmt(a.if_block, ctx)))
        else_comp = Bytecode.SEQ(*list(compile_stmt(a.else_block, ctx)))
        # If there is a return statement in either block, the skip must be a return skip to avoid another return statement executing
        if any(op.type == Bytecode.RETURN for op in itertools.chain(if_comp.linearize(), else_comp.linearize())):
            yield Bytecode.SETSKIP(0)
        else:
            yield Bytecode.SETSKIP(end_label)
        yield Bytecode.JUMP_IFNOT(else_label, compile_expr(a.expr, ctx))
        yield if_comp
        yield Bytecode.JUMP(end_label)
        yield else_label
        yield else_comp
        yield end_label
    elif isinstance(a, ast.WhileStmt):
        start_label, end_label = ctx.label(), ctx.label()
        ctx.push_loop(start_label, end_label)
        block_comp = Bytecode.SEQ(*list(compile_stmt(a.block, ctx)))
        # If there is a return statement in the block, the skip must be a return skip to avoid another return statement executing
        if any(op.type == Bytecode.RETURN for op in block_comp.linearize()):
            yield Bytecode.SETSKIP(0)
        else:
            yield Bytecode.SETSKIP(end_label)
        yield start_label
        yield Bytecode.JUMP_IFNOT(end_label, compile_expr(a.expr, ctx))
        yield from compile_stmt(a.block, ctx)
        yield Bytecode.JUMP(start_label)
        yield end_label
        ctx.pop_loop()
    elif isinstance(a, ast.ForStmt):
        return ...
    #elif isinstance(a, ast.):
    else:
        raise RuntimeError(f"Cannot compile {type(a)} to IR.")


def compile(a, fname):
    ctx = CompiledCode(fname)
    ctx.code = Bytecode.SEQ(*list(compile_stmt(a, ctx)), Bytecode.RETURN(ctx.const(0)))
    return ctx
