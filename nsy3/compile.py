import itertools
import struct

from . import ast, bytecode, serialisation
from .bytecode import Bytecode


RETURN_SKIP = HALF_INT_MAX = 2 ** 16 - 1

DOLLAR_GET_FLAGS = {
    "partial": 1
}

DOLLAR_SET_FLAGS = {
    "modification": 1,
    "default": 2
}

REFLECTED_BINOPS = ["+", "-", "*", "/", "//", "%", "**", "==", "!=", "<", ">", "<=", ">="]

class Combine:
    def __init__(self, a, b):
        self.a, self.b = a, b

    def __str__(self):
        return f"{self.a} | {self.b}"

    def __index__(self):
        return ((self.a.pos if isinstance(self.a, bytecode.BCode) else self.a) & 0xFFFF) + (self.b << 16)


class CompiledCode:
    def __init__(self, fname, modname):
        self.fname = fname
        self.modname = modname
        self.counter = itertools.count()
        self.variables = {}
        self.globals = {}
        self.label_counter = itertools.count()
        self.loop_stack = []
        self.consts = []
        self.functions = []
        self.code = None
        self.stacksave = 0
        self.imports = []

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
        for idx, c in enumerate(self.consts):
            if c == const and type(c) is type(const):
                break
        else:
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
        header = serialisation.serialise({
            "fname": str(self.fname.resolve()),
            "imports": self.imports,
            "name": self.modname
        })
        body = serialisation.serialise({
            "consts": [(c.pos if isinstance(c, bytecode.BCode) else c) for c in self.consts],
            "linenotab": linenotab,
            "code": b"".join(full_code.to_bytes())
        })
        return header + body

    def to_str(self):
        stuff = ["Consts:", str(self.consts), "", "Code:", self.code.to_str()]
        for i, function in enumerate(self.functions):
            stuff.extend(["", f"F{i}:", function.to_str()])
        return "\n".join(stuff)

    def add_import(self, name):
        if name[0] == ".":
            raise RuntimeError("IMP ...")
        absolute_name = ".".join(name)
        self.imports.append(absolute_name)
        return absolute_name

    def savestack(self, num):
        self.stacksave += num

    def setskip(self, pos):
        return Bytecode.SETSKIP(Combine(pos, self.stacksave))


def compile_expr_iter(a, ctx):
    if a.lineno:
        yield Bytecode.LINENO(a.lineno)
    if isinstance(a, ast.Binop):
        # Short circuiting
        if a.op == "and":
            end_label = ctx.label()
            yield compile_expr(a.left, ctx)
            yield Bytecode.JUMP_IFNOT_KEEP(end_label)
            yield Bytecode.DROP(1)
            yield compile_expr(a.right, ctx)
            yield end_label
        elif a.op == "or":
            end_label = ctx.label()
            yield compile_expr(a.left, ctx)
            yield Bytecode.JUMP_IF_KEEP(end_label)
            yield Bytecode.DROP(1)
            yield compile_expr(a.right, ctx)
            yield end_label
        elif a.op in REFLECTED_BINOPS:
            yield Bytecode.BINOP(ctx.const(a.op, wrap=False), compile_expr(a.left, ctx), compile_expr(a.right, ctx))
        else:
            yield Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.left, ctx), ctx.const(a.op)), compile_expr(a.right, ctx))
    elif isinstance(a, ast.Getattr):
        yield Bytecode.GETATTR(compile_expr(a.left, ctx), ctx.const(a.right))
    elif isinstance(a, ast.Monop):
        if a.op == "not":
            yield Bytecode.CALL(Bytecode.GET(ctx.const("not", wrap=False)), compile_expr(a.value, ctx))
        else:
            yield Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.value, ctx), ctx.const("u" + a.op)))
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
        if isinstance(a.seq, ast.CompExpr):
            if a.type == "{}" and isinstance(a.seq.expr, tuple):
                # Dict
                yield Bytecode.CALL(Bytecode.GET(ctx.const("Dict", wrap=False)), compile_expr(a.seq, ctx))
            elif a.type == "{}":
                # Set
                yield Bytecode.CALL(Bytecode.GET(ctx.const("Set", wrap=False)), compile_expr(a.seq, ctx))
            else:
                # List
                yield compile_expr(a.seq, ctx)
        else:
            lst = Bytecode.BUILDLIST(len(a.seq), *[compile_expr(s, ctx) for s in a.seq])

            if a.type == "{}" and (not a.seq or isinstance(a.seq[0], tuple)):
                # Dict
                yield Bytecode.CALL(Bytecode.GET(ctx.const("Dict", wrap=False)), lst)
            elif a.type == "{}":
                # Set
                yield Bytecode.CALL(Bytecode.GET(ctx.const("Set", wrap=False)), lst)
            else:
                # List
                yield lst
    elif isinstance(a, ast.Name):
        yield Bytecode.GET(ctx.const(a.name, wrap=False))
    elif isinstance(a, ast.DollarName):
        flags = 0
        for flag in a.flags:
            flags |= DOLLAR_GET_FLAGS[flag]
        yield Bytecode.CALL(Bytecode.GET(ctx.const("$?", wrap=False)), Bytecode.CALL(Bytecode.GET(ctx.const("[]", wrap=False)), *[compile_expr(n, ctx) for n in a.name]), ctx.const(flags))
    elif isinstance(a, ast.Func):
        func_label = ctx.add_function(Bytecode.SEQ(Bytecode.LINENO(a.lineno), *list(compile_stmt(a.stmt, ctx)), Bytecode.RETURN(ctx.const(None))))
        arg_names = []
        defaults = []
        for arg in a.args:
            arg_names.append(arg[0])
            if arg[1] is not None:
                defaults.append(compile_expr(arg[1], ctx))
        defaults_expr = Bytecode.CALL(Bytecode.GET(ctx.const("[]", wrap=False)), *defaults)
        signature = Bytecode.CALL(Bytecode.GET(ctx.const("Signature", wrap=False)), ctx.const(arg_names), defaults_expr, ctx.const(0))
        yield Bytecode.CALL(Bytecode.GET(ctx.const("->", wrap=False)), Bytecode.GET(ctx.const("__code__", wrap=False)), ctx.const(func_label), signature, Bytecode.GETENV())
    elif isinstance(a, ast.IfExpr):
        else_label, end_label = ctx.label(), ctx.label()
        yield compile_expr(a.cond, ctx)
        yield Bytecode.JUMP_IFNOT(else_label)
        yield compile_expr(a.left, ctx)
        yield Bytecode.JUMP(end_label)
        yield else_label
        yield compile_expr(a.right, ctx)
        yield end_label
    elif isinstance(a, ast.CompExpr):
        num_loops = len(ctx.loop_stack)
        code = [
            Bytecode.LINENO(a.expr.lineno),
            ctx.const([]),
            *[compile_expr(t, ctx) for t in a.trailers]
        ]
        code.extend([
            Bytecode.RROT(len(ctx.loop_stack) - num_loops),
            Bytecode.CALL(Bytecode.GETATTR(Bytecode.IGNORE(), ctx.const(":+")), compile_expr(a.expr, ctx)),
            Bytecode.ROT(len(ctx.loop_stack) - num_loops),
        ])
        while len(ctx.loop_stack) > num_loops:
            start_label, end_label = ctx.pop_loop()
            code.append(Bytecode.JUMP(start_label))
            code.append(end_label)
            code.append(Bytecode.DROP(1))
        code.append(Bytecode.RETURN(Bytecode.IGNORE()))
        func_label = ctx.add_function(Bytecode.SEQ(*code))
        signature = Bytecode.CALL(Bytecode.GET(ctx.const("Signature", wrap=False)), ctx.const([]), ctx.const([]), ctx.const(0))
        yield Bytecode.CALL(Bytecode.CALL(Bytecode.GET(ctx.const("->", wrap=False)), Bytecode.GET(ctx.const("__code__", wrap=False)), ctx.const(func_label), signature, Bytecode.GETENV()))
    elif isinstance(a, ast.CompForExpr):
        start_label, end_label = ctx.label(), ctx.label()
        ctx.push_loop(start_label, end_label)
        yield Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.expr, ctx), ctx.const("__iter__")))
        yield start_label
        yield Bytecode.CALL(Bytecode.GETATTR(Bytecode.IGNORE(), ctx.const("__next__")))
        yield Bytecode.JUMP_IFNOT_KEEP(end_label)
        yield Bytecode.UNPACK(Combine(2, HALF_INT_MAX))
        yield Bytecode.SET(ctx.const(a.name, wrap=False), Bytecode.IGNORE())
    elif isinstance(a, ast.CompIfExpr):
        yield compile_expr(a.cond, ctx)
        labels = ctx.current_loop()
        yield Bytecode.JUMP_IFNOT(labels[0])
    else: # pragma: no cover
        raise RuntimeError(f"Cannot compile {type(a)} to IR.")


def compile_expr(*args, **kwargs):
    instrs = list(compile_expr_iter(*args, **kwargs))
    if len(instrs) == 1:
        return instrs[0]
    return Bytecode.SEQ(*instrs)


def compile_stmt(a, ctx):
    if a.lineno:
        yield Bytecode.LINENO(a.lineno)
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
        yield ctx.setskip(RETURN_SKIP)
        yield Bytecode.RETURN(compile_expr(a.expr, ctx))
    elif isinstance(a, ast.Block):
        for s in a.stmts:
            yield from compile_stmt(s, ctx)
    elif isinstance(a, ast.ExprStmt):
        skip_label = ctx.label()
        yield ctx.setskip(skip_label)
        yield compile_expr(a.expr, ctx)
        yield Bytecode.DROP(1)
        yield skip_label
    elif isinstance(a, ast.AssignStmt):
        skip_label = ctx.label()
        yield ctx.setskip(skip_label)
        yield Bytecode.SET(ctx.const(a.name, wrap=False), compile_expr(a.expr, ctx))
        yield skip_label
    elif isinstance(a, ast.DollarSetStmt):
        skip_label = ctx.label()
        yield ctx.setskip(skip_label)
        flags = 0
        for flag in a.flags:
            flags |= DOLLAR_SET_FLAGS[flag]
        yield Bytecode.CALL(Bytecode.GET(ctx.const("$=", wrap=False)), Bytecode.CALL(Bytecode.GET(ctx.const("[]", wrap=False)), *[compile_expr(n, ctx) for n in a.name]), compile_expr(a.expr, ctx), ctx.const(flags))
        yield Bytecode.DROP(1)
        yield skip_label
    elif isinstance(a, ast.IfStmt):
        else_label, end_label = ctx.label(), ctx.label()
        if_comp = Bytecode.SEQ(*list(compile_stmt(a.if_block, ctx)))
        else_comp = Bytecode.SEQ(*list(compile_stmt(a.else_block, ctx)))
        # If there is a return statement in either block, the skip must be a return skip to avoid another return statement executing
        if any(op.type == Bytecode.RETURN for op in itertools.chain(if_comp.linearize(), else_comp.linearize())):
            yield ctx.setskip(RETURN_SKIP)
        else:
            yield ctx.setskip(end_label)
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
        yield start_label
        if any(op.type == Bytecode.RETURN for op in block_comp.linearize()):
            yield ctx.setskip(RETURN_SKIP)
        else:
            yield ctx.setskip(end_label)
        yield Bytecode.JUMP_IFNOT(end_label, compile_expr(a.expr, ctx))
        yield from compile_stmt(a.block, ctx)
        yield Bytecode.JUMP(start_label)
        yield end_label
        ctx.pop_loop()
    elif isinstance(a, ast.Assert):
        skip_label = ctx.label()
        yield ctx.setskip(skip_label)
        yield Bytecode.CALL(Bytecode.GET(ctx.const("assert", wrap=False)), compile_expr(a.expr, ctx))
        yield Bytecode.DROP(1)
        yield skip_label
    elif isinstance(a, ast.ImportStmt):
        skip_label = ctx.label()
        modname = ctx.add_import(a.modname)
        if a.names is None:
            yield Bytecode.SET(ctx.const(a.modname[0], wrap=False), Bytecode.CALL(Bytecode.GET(ctx.const("import", wrap=False)), ctx.const(a.modname[0])))
        else:
            yield Bytecode.CALL(Bytecode.GET(ctx.const("import", wrap=False)), ctx.const(modname))
            yield Bytecode.DUP(len(a.names) - 1)
            ctx.savestack(len(a.names))
            for modname, varname in a.names:
                ctx.savestack(-1)
                if modname == "_":
                    yield Bytecode.SET(ctx.const(varname, wrap=False), Bytecode.IGNORE())
                else:
                    # This is a name = mod.name where we know that mod is not a thunk and so thunks can be ignored.
                    # This is not taken advantage of, but might be in the future.
                    skip_label = ctx.label()
                    yield ctx.setskip(skip_label)
                    yield Bytecode.SET(ctx.const(varname, wrap=False), Bytecode.GETATTR(Bytecode.IGNORE(), ctx.const(modname)))
                    yield skip_label
    elif isinstance(a, ast.ForStmt):
        start_label, end_label = ctx.label(), ctx.label()
        full_end_label = ctx.label()
        ctx.push_loop(start_label, end_label)
        ctx.savestack(1)
        block_comp = Bytecode.SEQ(*list(compile_stmt(a.block, ctx)))
        # If there is a return statement in the block, the skip must be a return skip to avoid another return statement executing
        yield start_label
        return_inside_block = any(op.type == Bytecode.RETURN for op in block_comp.linearize())
        inner_setskip = ctx.setskip(RETURN_SKIP if return_inside_block else end_label)
        ctx.savestack(-1)
        outer_setskip = ctx.setskip(RETURN_SKIP if return_inside_block else full_end_label)
        yield outer_setskip
        yield Bytecode.CALL(Bytecode.GETATTR(compile_expr(a.expr, ctx), ctx.const("__iter__")))
        yield start_label
        yield outer_setskip
        yield Bytecode.CALL(Bytecode.GETATTR(Bytecode.IGNORE(), ctx.const("__next__")))
        yield Bytecode.JUMP_IFNOT_KEEP(end_label)
        yield inner_setskip
        yield Bytecode.UNPACK(Combine(2, HALF_INT_MAX))
        yield Bytecode.SET(ctx.const(a.name, wrap=False), Bytecode.IGNORE())
        yield block_comp
        yield Bytecode.JUMP(start_label)
        yield end_label
        yield Bytecode.DROP(1)
        yield full_end_label
        ctx.pop_loop()
    else: # pragma: no cover
        raise RuntimeError(f"Cannot compile {type(a)} to IR.")


def compile(a, fname, modname):
    ctx = CompiledCode(fname, modname)
    ctx.code = Bytecode.SEQ(*list(compile_stmt(a, ctx)), Bytecode.RETURN(ctx.const(None)))
    return ctx
