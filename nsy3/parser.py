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
    tokens = {NAME, AT, DOLLER, DOLLERDOLLER, NUMBER, STRING, DOT, COMMA, EQEQ, ARROW, LAMBDA, NEQ, EQ,
              PLUSEQ, PLUS, MINUSEQ, MINUS, STAREQ, STAR, STARSTAREQ, STARSTAR, SLASHEQ, SLASH, SLASHSLASHEQ, SLASHSLASH, PERCENTEQ, PERCENT,
              LBRAK, RBRAK, LPAREN, RPAREN, LCURLY, RCURLY, LTE, GTE, LT, GT, COLON, COLONPLUS, COLONPLUSEQ, TRUE, FALSE, IF, ELSE, ELIF, FOR, WHILE,
              IN, AND, OR, NOT, DEF, RETURN, BREAK, CONTINUE, PASS, ASSERT, IMPORT, FROM, AS, WHITESPACE, INDENT, DEDENT, NEWLINE}

    NAME = r"[a-zA-Z_][a-zA-Z_0-9']*"
    AT = r"@"
    DOLLERDOLLER = r"\$\$"
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
    STARSTAREQ = r"\*\*="
    STARSTAR = r"\*\*"
    STAREQ = r"\*="
    STAR = r"\*"
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
    COLONPLUSEQ = r":\+="
    COLONPLUS = r":\+"
    COLON = r":"
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
    NAME["assert"] = ASSERT
    NAME["import"] = IMPORT
    NAME["from"] = FROM
    NAME["as"] = AS

    @_(r"[0-9]+(\.[0-9]+)?")
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

    @_(r'([ \t\n]|\#.*\n)+')
    def WHITESPACE(self, t):
        self.lineno += t.value.count("\n")
        return t

    def error(self, t):
        raise RuntimeError(f"Illegal character {t.value[0]!r}")

    def tokenize(self, *args, **kwargs):
        indent_stack = [0]
        ends_with_newline = False
        first_token = True
        for token in super().tokenize(*args, **kwargs):
            ends_with_newline = False
            if token.type == "WHITESPACE":
                if "\n" not in token.value:
                    continue
                before, indentation = token.value.rsplit("\n", 1)

                if not first_token:
                    t = sly.lex.Token()
                    t.value = "\n"
                    t.type = "NEWLINE"
                    t.lineno = token.lineno + before.count("\n")
                    t.index = token.index + len(before)
                    yield t

                    ends_with_newline = True

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

            first_token = False

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
        ("nonassoc", LOWPREC),
        ("left", ARROW, LAMBDA),
        ("right", IFEXPR, IF, ELSE),
        ("left", COMP),
        ("left", OR),
        ("left", AND),
        ("right", NOT),
        ("nonassoc", LTE, GTE, LT, GT, EQEQ, NEQ),
        ("left", COLONPLUS),
        ("left", PLUS, MINUS),
        ("left", STAR, SLASH, SLASHSLASH, PERCENT),
        ("right", UMINUS),
        ("right", STARSTAR),
        ("left", LBRAK, DOT, LPAREN)
    )

    @_("NEWLINE")
    def initial(self, p):
        return ast.Block(p, [])

    @_("stmts")
    def initial(self, p):
        return ast.Block(p, p.stmts)

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
        return ast.AssignStmt(p, p.NAME, p.expr)

    @_("expr")
    def simple_stmt(self, p):
        return ast.ExprStmt(p, p.expr)

    #@_("NAME EQ expr SEMI")
    #def stmt(self, p):
        #return ast.MultiAssignStmt(p.NAME, p.expr)

    @_("IF expr block if_trailer")
    def stmt(self, p):
        return ast.IfStmt(p, p.expr, p.block, p.if_trailer)

    @_("")
    def if_trailer(self, p):
        return ast.Pass(p)

    @_("ELSE block")
    def if_trailer(self, p):
        return p.block

    @_("ELIF expr block if_trailer")
    def if_trailer(self, p):
        return ast.IfStmt(p, p.expr, p.block, p.if_trailer)

    @_("FOR NAME IN expr block")
    def stmt(self, p):
        return ast.ForStmt(p, p.NAME, p.expr, p.block)

    @_("WHILE expr block")
    def stmt(self, p):
        return ast.WhileStmt(p, p.expr, p.block)

    @_("PASS")
    def simple_stmt(self, p):
        return ast.Pass(p)

    @_("CONTINUE")
    def simple_stmt(self, p):
        return ast.Continue(p)

    @_("BREAK")
    def simple_stmt(self, p):
        return ast.Break(p)

    @_("RETURN expr")
    def simple_stmt(self, p):
        return ast.Return(p, p.expr)

    @_("ASSERT expr")
    def simple_stmt(self, p):
        return ast.Assert(p, p.expr)

    @_("COLON NEWLINE INDENT stmts DEDENT")
    def block(self, p):
        return ast.Block(p, p.stmts)

    @_("DEF NAME LPAREN defargs RPAREN block")
    def stmt(self, p):
        return ast.AssignStmt(p, p.NAME, ast.Func(p, p.defargs, p.block))

    @_("IMPORT dotsmodname AS NAME")
    def stmt(self, p):
        return ast.ImportStmt(p, p.dotsmodname, [("*", p.NAME)])

    @_("FROM dotsmodname IMPORT modfromnames")
    def stmt(self, p):
        return ast.ImportStmt(p, p.dotsmodname, p.modfromnames)

    @_("NAME")
    def modfromname(self, p):
        return (p.NAME, p.NAME)

    @_("NAME AS NAME")
    def modfromname(self, p):
        return (p.NAME0, p.NAME1)

    @_("STAR AS NAME")
    def modfromname(self, p):
        return ("*", p.NAME)

    @_("modfromname")
    def modfromnames(self, p):
        return [p.modfromname]

    @_("modfromnames COMMA modfromname")
    def modfromnames(self, p):
        return p.modfromnames + [p.modfromname]

    @_("DOT")
    def dots(self, p):
        return ["."]

    @_("dots DOT")
    def dots(self, p):
        return p.dots + ["."]

    @_("dots")
    def dotsmodname(self, p):
        return p.dots

    @_("dots modname")
    def dotsmodname(self, p):
        return p.dots + p.modname

    @_("modname")
    def dotsmodname(self, p):
        return p.modname

    @_("NAME")
    def modname(self, p):
        return [p.NAME]

    @_("modname DOT NAME")
    def modname(self, p):
        return p.modname + [p.NAME]

    @_("expr IF expr ELSE expr %prec IFEXPR")
    def expr(self, p):
        return ast.IfExpr(p, p.expr0, p.expr1, p.expr2)

    @_("LAMBDA defargs ARROW expr %prec ARROW")
    def expr(self, p):
        return ast.Func(p, p.defargs, ast.Return(p, p.expr))

    @_("LPAREN expr RPAREN")
    def expr(self, p):
        return p.expr

    @_("NAME")
    def expr(self, p):
        return ast.Name(p, p.NAME)

    @_("expr LPAREN args RPAREN %prec LPAREN")
    def expr(self, p):
        return ast.Call(p, p.expr, p.args)

    @_("NUMBER")
    def expr(self, p):
        return ast.Literal(p, p.NUMBER)

    @_("STRING")
    def expr(self, p):
        return ast.Literal(p, p.STRING)

    @_("TRUE", "FALSE")
    def expr(self, p):
        return ast.Literal(p, p[0] == "true")

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
        "expr OR expr",

        # Seqs
        "expr COLONPLUS expr",
    )
    def expr(self, p):
        return ast.Binop(p, p[1], p.expr0, p.expr1)

    @_(
        # Arith
        "MINUS expr %prec UMINUS",

        # Logs
        "NOT expr",
    )
    def expr(self, p):
        return ast.Monop(p, p[0], p.expr)


    @_("expr LBRAK expr RBRAK %prec LBRAK")
    def expr(self, p):
        return ast.Binop(p, "[]", p.expr0, p.expr1)

    @_("expr DOT NAME %prec DOT")
    def expr(self, p):
        return ast.Getattr(p, p.expr, p.NAME)

    @_(
        # Arith
        "NAME PLUSEQ expr",
        "NAME MINUSEQ expr",
        "NAME STAREQ expr",
        "NAME SLASHEQ expr",
        "NAME SLASHSLASHEQ expr",
        "NAME PERCENTEQ expr",
        "NAME STARSTAREQ expr",

        # Seqs
        "NAME COLONPLUSEQ expr",
    )
    def simple_stmt(self, p):
        return ast.AssignStmt(p, p[0], ast.Binop(p, p[1][:-1], ast.Name(p, p[0]), p.expr))

    @_(
        # Arith
        "DOLLER multipartname dflags DOLLER PLUSEQ expr",
        "DOLLER multipartname dflags DOLLER MINUSEQ expr",
        "DOLLER multipartname dflags DOLLER STAREQ expr",
        "DOLLER multipartname dflags DOLLER SLASHEQ expr",
        "DOLLER multipartname dflags DOLLER SLASHSLASHEQ expr",
        "DOLLER multipartname dflags DOLLER PERCENTEQ expr",
        "DOLLER multipartname dflags DOLLER STARSTAREQ expr",
        "DOLLER multipartname dflags DOLLER COLONPLUSEQ expr",
    )
    def simple_stmt(self, p):
        old_value = ast.DollarName(p, p.multipartname, ["partial"])
        return ast.DollarSetStmt(p, p.multipartname, ast.Binop(p, p[4][:-1], old_value, p.expr), ["modification"])

    @_("DOLLER multipartname dflags DOLLER EQ expr")
    def simple_stmt(self, p):
        return ast.DollarSetStmt(p, p.multipartname, p.expr, p.dflags)

    @_("DOLLER multipartname dflags DOLLER")
    def expr(self, p):
        return ast.DollarName(p, p.multipartname, p.dflags)

    @_("DOLLERDOLLER multipartname DOLLER")
    def expr(self, p):
        return ast.SequenceLiteral(p, "[]", p.multipartname)

    @_("NAME")
    def multipartname(self, p):
        return [ast.Literal(p, p.NAME)]

    @_("multipartname DOT NAME")
    def multipartname(self, p):
        return p.multipartname + [ast.Literal(p, p.NAME)]

    @_("multipartname LBRAK expr RBRAK")
    def multipartname(self, p):
        return p.multipartname + [p.expr]

    # So, there is an ambiguity, where $a.b.c could be ($a.b).c or ($a.b.c). We solve this using the precedence table.
    # However, why the LOWPREC needs to be assigned to this rule? No idea.
    @_("%prec LOWPREC")
    def dflags(self, p):
        return []

    @_("dflags AT NAME")
    def dflags(self, p):
        return p.dflags + [p.NAME]

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
        return ast.SequenceLiteral(p, "[]", p.seq)

    @_("LCURLY seq RCURLY")
    def expr(self, p):
        return ast.SequenceLiteral(p, "{}", p.seq)

    @_("")
    def seq(self, p):
        return []

    @_("expr")
    def seq(self, p):
        return [p.expr]

    @_("comp")
    def seq(self, p):
        return p.comp

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

    @_("expr comp_trailer")
    def comp(self, p):
        return ast.CompExpr(p, p.expr, p.comp_trailer)

    @_("expr COLON expr comp_trailer")
    def comp(self, p):
        return ast.CompExpr(p, (p.expr0, p.expr1), p.comp_trailer)

    @_("comp_for")
    def comp_trailer(self, p):
        return [p.comp_for]

    @_("comp_trailer comp_for")
    def comp_trailer(self, p):
        return p.comp_trailer + [p.comp_for]

    @_("comp_trailer comp_if")
    def comp_trailer(self, p):
        return p.comp_trailer + [p.comp_if]

    @_("FOR NAME IN expr %prec COMP")
    def comp_for(self, p):
        return ast.CompForExpr(p, p.NAME, p.expr)

    @_("IF expr %prec COMP")
    def comp_if(self, p):
        return ast.CompIfExpr(p, p.expr)

    def error(self, p):
        print(self.__dict__)
        raise RuntimeError(f"Invalid syntax {p}")


def parse(text):
    return NSY2Parser().parse(NSY2Lexer().tokenize(text))
