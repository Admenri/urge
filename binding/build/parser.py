import re
from typing import List, Optional


class Token:
    def __init__(self, kind: str, value: str, line: int = 0):
        self.kind = kind
        self.value = value
        self.line = line

    def __repr__(self):
        return f"Token({self.kind}, {self.value!r})"


class IDLParser:
    KEYWORDS = frozenset({
        "class", "struct", "enum", "attribute", "static", "filepath",
    })

    _TOKEN_SPEC = [
        ("COMMENT_LINE", r"//[^\n]*"),
        ("COMMENT_BLOCK", r"/\*[\s\S]*?\*/"),
        ("STRING", r'"[^"]*"'),
        ("NUMBER", r"\d+"),
        ("IDENT", r"[a-zA-Z_][a-zA-Z0-9_]*"),
        ("LBRACE", r"\{"),
        ("RBRACE", r"\}"),
        ("LPAREN", r"\("),
        ("RPAREN", r"\)"),
        ("COLON", r":"),
        ("COMMA", r","),
        ("SEMICOLON", r";"),
        ("LT", r"<"),
        ("GT", r">"),
        ("WS", r"\s+"),
    ]

    _TOKEN_RE = re.compile(
        "|".join(f"(?P<{name}>{pat})" for name, pat in _TOKEN_SPEC)
    )

    def __init__(self):
        self._tokens: List[Token] = []
        self._pos: int = 0

    def parse(self, content: str) -> dict:
        self._tokens = self._tokenize(content)
        self._pos = 0
        return self._parse_root()

    def _tokenize(self, content: str) -> List[Token]:
        tokens: List[Token] = []
        line = 1
        for m in self._TOKEN_RE.finditer(content):
            kind = m.lastgroup
            value = m.group()
            if kind in ("WS", "COMMENT_LINE", "COMMENT_BLOCK"):
                line += value.count("\n")
                continue
            if kind == "IDENT" and value in self.KEYWORDS:
                kind = value.upper()
            tokens.append(Token(kind, value, line))
        return tokens

    def _cur(self) -> Optional[Token]:
        if self._pos < len(self._tokens):
            return self._tokens[self._pos]
        return None

    def _peek(self, offset: int = 0) -> Optional[Token]:
        i = self._pos + offset
        if i < len(self._tokens):
            return self._tokens[i]
        return None

    def _advance(self) -> Optional[Token]:
        tok = self._cur()
        if tok is not None:
            self._pos += 1
        return tok

    def _expect(self, kind: str) -> Token:
        tok = self._cur()
        if tok is None or tok.kind != kind:
            got = tok.kind if tok else "EOF"
            ln = tok.line if tok else "EOF"
            raise SyntaxError(
                f"Expected {kind}, got {got} at line {ln}"
            )
        return self._advance()

    def _match(self, kind: str) -> Optional[Token]:
        tok = self._cur()
        if tok is not None and tok.kind == kind:
            return self._advance()
        return None

    def _parse_type(self) -> str:
        result = self._expect("IDENT").value

        while (
            self._cur() is not None
            and self._cur().kind == "COLON"
            and self._peek(1) is not None
            and self._peek(1).kind == "COLON"
        ):
            self._advance()
            self._advance()
            result += "::" + self._expect("IDENT").value

        if self._cur() is not None and self._cur().kind == "LT":
            self._advance()
            result += "<" + self._parse_type()
            while self._cur() is not None and self._cur().kind == "COMMA":
                self._advance()
                result += ", " + self._parse_type()
            self._expect("GT")
            result += ">"

        return result

    def _parse_root(self) -> dict:
        result = {"class": {}, "struct": {}}
        while self._cur() is not None:
            tok = self._cur()
            if tok.kind == "CLASS":
                self._advance()
                self._parse_class(result)
            elif tok.kind == "STRUCT":
                self._advance()
                self._parse_struct(result)
            else:
                self._advance()
        return result

    def _parse_class(self, result: dict):
        name = self._expect("IDENT").value

        parent = "Object"
        if self._match("COLON"):
            parent = self._parse_type()

        self._expect("LBRACE")

        info = {
            "filepath": "",
            "parent": parent,
            "members": {},
            "attributes": {},
            "enums": {},
        }

        while self._cur() is not None and self._cur().kind != "RBRACE":
            tok = self._cur()
            if tok.kind == "FILEPATH":
                self._advance()
                info["filepath"] = self._expect("STRING").value.strip('"')
            elif tok.kind == "STATIC":
                self._advance()
                self._parse_member(info, static=True)
            elif tok.kind == "ATTRIBUTE":
                self._advance()
                self._parse_attribute(info)
            elif tok.kind == "ENUM":
                self._advance()
                self._parse_enum(info)
            elif tok.kind == "IDENT":
                self._parse_member(info, static=False)
            else:
                self._advance()

        self._expect("RBRACE")
        result["class"][name] = info

    def _parse_member(self, info: dict, static: bool = False):
        ret_type = self._parse_type()
        name = self._expect("IDENT").value

        self._expect("LPAREN")
        params = self._parse_params()
        self._expect("RPAREN")

        self._match("SEMICOLON")

        info["members"][name] = {
            "static": static,
            "params": params,
            "return": ret_type,
        }

    def _parse_params(self) -> list:
        params = []
        if self._cur() is not None and self._cur().kind != "RPAREN":
            params.append(self._parse_param())
            while self._match("COMMA"):
                params.append(self._parse_param())
        return params

    def _parse_param(self) -> dict:
        typ = self._parse_type()
        name = self._expect("IDENT").value
        return {"name": name, "type": typ}

    def _parse_attribute(self, info: dict):
        typ = self._parse_type()
        name = self._expect("IDENT").value
        self._match("SEMICOLON")
        info["attributes"][name] = {"value": typ}

    def _parse_enum(self, info: dict):
        name = self._expect("IDENT").value

        range_type = "int32_t"
        if self._match("COLON"):
            range_type = self._parse_type()

        self._expect("LBRACE")

        members = []
        while self._cur() is not None and self._cur().kind != "RBRACE":
            members.append(self._expect("IDENT").value)
            self._match("COMMA")

        self._expect("RBRACE")

        info["enums"][name] = {
            "range": range_type,
            "members": members,
        }

    def _parse_struct(self, result: dict):
        name = self._expect("IDENT").value
        self._expect("LBRACE")

        info = {
            "filepath": "",
            "params": [],
        }

        while self._cur() is not None and self._cur().kind != "RBRACE":
            tok = self._cur()
            if tok.kind == "FILEPATH":
                self._advance()
                info["filepath"] = self._expect("STRING").value.strip('"')
            elif tok.kind == "IDENT":
                typ = self._parse_type()
                field = self._expect("IDENT").value
                self._match("SEMICOLON")
                info["params"].append({"name": field, "type": typ})
            else:
                self._advance()

        self._expect("RBRACE")
        result["struct"][name] = info
