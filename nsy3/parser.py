from sly import Lexer, Parser
import sly.lex
import re
from . import ast


SINGLE_STRING_ESCAPES = {
    "\\": "\\",
    "'": "'",
    "\"": "\"",
    "a": "\a",
    "b": "\b",
    "f": "\f",
    "n": "\n",
    "r": "\r",
    "t": "\t",
    "v": "\v",
}


def replace(match):
    txt = match.group(1)
    if txt in SINGLE_STRING_ESCAPES:
        return SINGLE_STRING_ESCAPES[txt]
    elif txt.isdigit():
        return chr(int(txt, 8))
    elif txt.startswith("u") or txt.startswith("U"):
        return chr(int(txt[1:], 16))
    elif txt.startswith("x"):
        return bytes([int(txt[1:], 16)]).decode("latin-1")
    return "\\" + txt


def decode_string(str):
    return re.sub(r"\\(u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8}|x[0-9A-Fa-f]{2}|[0-7]{1,3}|.)", replace, str)


class NSY2Lexer(Lexer):
    tokens = {NAME, AT, DOLLER, NUMBER, STRING, DOT, COMMA, EQEQ, ARROW, LAMBDA, NEQ, EQ,
              PLUSEQ, PLUS, MINUSEQ, MINUS, STAREQ, STAR, STARSTAREQ, STARSTAR, SLASHEQ, SLASH, SLASHSLASHEQ, SLASHSLASH, PERCENTEQ, PERCENT,
              LBRAK, RBRAK, LPAREN, RPAREN, LCURLY, RCURLY, LTE, GTE, LT, GT, COLON, TRUE, FALSE, IF, ELSE, ELIF, FOR, WHILE,
              IN, AND, OR, NOT, DEF, RETURN, BREAK, CONTINUE, PASS, WHITESPACE, INDENT, DEDENT, NEWLINE}

    NAME = r"[a-zA-Z_]\w*"
    AT = r"@"
    DOLLER = r"\$"
    EQEQ = r"=="
    ARROW = r"->"
    LAMBDA = r"\\\\"
    NEQ = r"!="
    EQ = r"="
    PLUSEQ = r"\+="
    PLUS = r"\+"
    MINUSEQ = r"-="
    MINUS = r"-"
    STAREQ = r"\*="
    STAR = r"\*"
    STARSTAREQ = r"\*\*="
    STARSTAR = r"\*\*"
    SLASHSLASHEQ = r"//="
    SLASHSLASH = r"//"
    SLASHEQ = r"/="
    SLASH = r"/"
    PERCENTEQ = r"%="
    PERCENT = r"%"
    DOT = r"\."
    COMMA = r","
    LBRAK = r"\["
    RBRAK = r"\]"
    LPAREN = r"\("
    RPAREN = r"\)"
    LCURLY = r"\{"
    RCURLY = r"\}"
    LTE = r"<="
    GTE = r">="
    LT = r"<"
    GT = r">"
    COLON = ":"
    NAME["true"] = TRUE
    NAME["false"] = FALSE
    NAME["if"] = IF
    NAME["else"] = ELSE
    NAME["elif"] = ELIF
    NAME["for"] = FOR
    NAME["while"] = WHILE
    NAME["in"] = IN
    NAME["and"] = AND
    NAME["or"] = OR
    NAME["not"] = NOT
    NAME["def"] = DEF
    NAME["return"] = RETURN
    NAME["break"] = BREAK
    NAME["continue"] = CONTINUE
    NAME["pass"] = PASS

    @_(r"[0-9]+(\.[0-9])?")
    def NUMBER(self, t):
        if "." in t.value:
            t.value = float(t.value)
        else:
            t.value = int(t.value)
        return t

    @_(r"\"([^\"]|\\.)*\"")
    def STRING(self, t):
        print(t.value)
        t.value = decode_string(t.value[1:-1])
        self.lineno += t.value.count("\n")
        return t

    @_(r'[ \t\n]+')
    def WHITESPACE(self, t):
        self.lineno += t.value.count("\n")
        return t

    ignore_comment = r'[ \t\n]*\#.*\n[\s\n]*'

    def error(self, t):
        raise RuntimeError(f"Illegal character {t.value[0]!r}")

    def tokenize(self, *args, **kwargs):
        indent_stack = [0]
        ends_with_newline = False
        for token in super().tokenize(*args, **kwargs):
            ends_with_newline = False
            if token.type == "WHITESPACE":
                if "\n" not in token.value:
                    continue
                before, indentation = token.value.rsplit("\n", 1)
                ends_with_newline = True

                t = sly.lex.Token()
                t.value = "\n"
                t.type = "NEWLINE"
                t.lineno = token.lineno + before.count("\n")
                t.index = token.index + len(before)
                yield t


                if self.index >= len(self.text):
                    continue

                while len(indentation) < indent_stack[-1]:
                    indent_stack.pop()
                    t = sly.lex.Token()
                    t.value = ""
                    t.type = "DEDENT"
                    t.lineno = token.lineno + token.value.count("\n")
                    t.index = token.index + len(token.value)
                    yield t

                if len(indentation) > indent_stack[-1]:
                    t = sly.lex.Token()
                    t.value = indentation[-len(indentation) + indent_stack[-1]:]
                    t.type = "INDENT"
                    t.lineno = token.lineno + token.value.count("\n")
                    t.index = token.index + len(token.value) - len(indentation) + indent_stack[-1]
                    yield t
                    indent_stack.append(len(indentation))
                if len(indentation) != indent_stack[-1]:
                    raise RuntimeError()
            else:
                yield token
        if indent_stack[-1] > 0:
            ends_with_newline = False
            while indent_stack[-1] > 0:
                indent_stack.pop()
                t = sly.lex.Token()
                t.value = ""
                t.type = "DEDENT"
                t.lineno = self.lineno
                t.index = self.index
                yield t

        if not ends_with_newline:
            t = sly.lex.Token()
            t.value = ""
            t.type = "NEWLINE"
            t.lineno = self.lineno
            t.index = self.index
            yield t



class NSY2Parser(Parser):
    debugfile = "parser.out"
    tokens = NSY2Lexer.tokens

    precedence = (
        ("right", ARROW),
        ("left", OR),
        ("left", AND),
        ("right", NOT),
        ("nonassoc", LTE, GTE, LT, GT, EQEQ, NEQ),
        ("left", PLUS, MINUS),
        ("left", STAR, SLASH, SLASHSLASH, PERCENT),
        ("right", UMINUS),
        ("right", STARSTAR),
        ("left", LBRAK)
    )

    @_("NEWLINE")
    def initial(self, p):
        return ast.Block([])

    @_("stmts")
    def initial(self, p):
        return ast.Block(p.stmts)

    @_("stmt")
    def stmts(self, p):
        return (p.stmt,)

    @_("stmts stmt")
    def stmts(self, p):
        return p.stmts + (p.stmt,)

    @_("simple_stmt NEWLINE")
    def stmt(self, p):
        return p.simple_stmt

    @_("stmt NEWLINE")
    def stmt(self, p):
        return p.stmt

    @_("NAME EQ expr")
    def simple_stmt(self, p):
        return ast.AssignStmt(p.NAME, p.expr)

    @_("expr")
    def simple_stmt(self, p):
        return ast.ExprStmt(p.expr)

    #@_("NAME EQ expr SEMI")
    #def stmt(self, p):
        #return ast.MultiAssignStmt(p.NAME, p.expr)

    @_("IF expr block if_trailer")
    def stmt(self, p):
        return ast.IfStmt(p.expr, p.block, p.if_trailer)

    @_("")
    def if_trailer(self, p):
        return ast.Pass()

    @_("ELSE block")
    def if_trailer(self, p):
        return p.block

    @_("ELIF expr block if_trailer")
    def if_trailer(self, p):
        return ast.IfStmt(p.expr, p.block, p.if_trailer)

    @_("FOR NAME IN expr block")
    def stmt(self, p):
        return ast.ForStmt(p.NAME, p.expr, p.block)

    @_("WHILE expr block")
    def stmt(self, p):
        return ast.WhileStmt(p.expr, p.block)

    @_("PASS")
    def simple_stmt(self, p):
        return ast.Pass()

    @_("CONTINUE")
    def simple_stmt(self, p):
        return ast.Continue()

    @_("BREAK")
    def simple_stmt(self, p):
        return ast.Break()

    @_("RETURN expr")
    def simple_stmt(self, p):
        return ast.Return(p.expr)

    @_("COLON NEWLINE INDENT stmts DEDENT")
    def block(self, p):
        return ast.Block(p.stmts)

    @_("LAMBDA defargs ARROW expr")
    def expr(self, p):
        return ast.Func(p.defargs, ast.Return(p.expr))

    @_("DEF NAME LPAREN defargs RPAREN block")
    def stmt(self, p):
        return ast.AssignStmt(p.NAME, ast.Func(p.defargs, p.block))

    @_("LPAREN expr RPAREN")
    def expr(self, p):
        return p.expr

    @_("NAME")
    def expr(self, p):
        return ast.Name(p.NAME)

    @_("expr LPAREN args RPAREN")
    def expr(self, p):
        return ast.Call(p.expr, p.args)

    @_("NUMBER")
    def expr(self, p):
        return ast.Literal(p.NUMBER)

    @_("STRING")
    def expr(self, p):
        return ast.Literal(p.STRING)

    @_("TRUE", "FALSE")
    def expr(self, p):
        return ast.Literal(p[0] == "true")

    @_(
        # Arith
        "expr PLUS expr",
        "expr MINUS expr",
        "expr STAR expr",
        "expr SLASH expr",
        "expr SLASHSLASH expr",
        "expr PERCENT expr",
        "expr STARSTAR expr",

        # Cmps
        "expr LTE expr",
        "expr GTE expr",
        "expr LT expr",
        "expr GT expr",
        "expr EQEQ expr",
        "expr NEQ expr",

        # Logs
        "expr AND expr",
        "expr OR expr"
    )
    def expr(self, p):
        return ast.Binop(p[1], p.expr0, p.expr1)

    @_(
        # Arith
        "MINUS expr %prec UMINUS",

        # Logs
        "NOT expr",
    )
    def expr(self, p):
        return ast.Monop(p[0], p.expr)


    @_("expr LBRAK expr RBRAK")
    def expr(self, p):
        return ast.Binop("[]", p.expr0, p.expr1)

    @_(
        # Arith
        "NAME PLUSEQ expr",
        "NAME MINUSEQ expr",
        "NAME STAREQ expr",
        "NAME SLASHEQ expr",
        "NAME SLASHSLASHEQ expr",
        "NAME PERCENTEQ expr",
        "NAME STARSTAREQ expr",
    )
    def simple_stmt(self, p):
        return ast.AssignStmt(p[0], ast.Call(ast.Name(p[1][:-1]), [ast.Name(p[0]), p.expr]))

    @_(
        # Arith
        "DOLLER multipartname PLUSEQ expr",
        "DOLLER multipartname MINUSEQ expr",
        "DOLLER multipartname STAREQ expr",
        "DOLLER multipartname SLASHEQ expr",
        "DOLLER multipartname SLASHSLASHEQ expr",
        "DOLLER multipartname STARSTAREQ expr",
    )
    def simple_stmt(self, p):
        old_value = ast.Call(ast.Name("$?"), [ast.SequenceLiteral("[]", p.multipartname), ast.SequenceLiteral("[]", [ast.Literal("partial")])])
        return ast.ExprStmt(ast.Call(ast.Name("$="), [ast.SequenceLiteral("[]", p.multipartname),
                                                      ast.Call(ast.Name(p[2][:-1]), [old_value, p.expr])]))

    @_("DOLLER multipartname EQ expr")
    def simple_stmt(self, p):
        return ast.ExprStmt(ast.Call(ast.Name("$="), [ast.SequenceLiteral("[]", p.multipartname), p.expr]))

    @_("DOLLER multipartname dflags")
    def expr(self, p):
        return ast.Call(ast.Name("$?"), [ast.SequenceLiteral("[]", p.multipartname), ast.SequenceLiteral("[]", p.dflags)])

    @_("NAME")
    def multipartname(self, p):
        return [ast.Literal(p.NAME)]

    @_("multipartname DOT NAME")
    def multipartname(self, p):
        return p.multipartname + [ast.Literal(p.NAME)]

    @_("multipartname LBRAK expr RBRAK")
    def multipartname(self, p):
        return p.multipartname + [p.expr]

    @_("")
    def dflags(self, p):
        return []

    @_("dflags AT NAME")
    def dflags(self, p):
        return p.dflags + [ast.Literal(p.NAME)]

    @_("")
    def args(self, p):
        return ()

    @_("arg")
    def args(self, p):
        return (p.arg,)

    @_("args COMMA arg")
    def args(self, p):
        return p.args + (p.arg,)

    @_("expr")
    def arg(self, p):
        return p.expr

    @_("NAME EQ expr")
    def arg(self, p):
        return (p.NAME, p.expr)

    @_("")
    def defargs(self, p):
        return ()

    @_("defarg")
    def defargs(self, p):
        return (p.defarg,)

    @_("defargs COMMA defarg")
    def defargs(self, p):
        return p.defargs + (p.defarg,)

    @_("NAME")
    def defarg(self, p):
        return (p.NAME, None)

    @_("NAME EQ expr")
    def defarg(self, p):
        return (p.NAME, p.expr)

    @_("LBRAK seq RBRAK")
    def expr(self, p):
        return ast.SequenceLiteral("[]", p.seq)

    @_("LCURLY seq RCURLY")
    def expr(self, p):
        return ast.SequenceLiteral("{}", p.seq)

    @_("")
    def seq(self, p):
        return []

    @_("expr")
    def seq(self, p):
        return [p.expr]

    @_("seq COMMA expr")
    def seq(self, p):
        p.seq.append(p.expr)
        return p.seq

    @_("expr COLON expr")
    def seq(self, p):
        return [(p.expr0, p.expr1)]

    @_("seq COMMA expr COLON expr")
    def seq(self, p):
        p.seq.append((p.expr0, p.expr1))
        return p.seq

    def error(self, p):
        print(self.__dict__)
        raise RuntimeError(f"Invalid syntax {p}")


def parse(text):
    return NSY2Parser().parse(NSY2Lexer().tokenize(text))
