"""Declaration reader for include/acsd/model.hpp.

The typed input structs, their TypeIds, JSON schemas and codecs are generated
from the *declarations*, not from a second handwritten field list.  This module
dumps the Clang AST of include/acsd/model.hpp and exposes the pieces the
generator needs:

  * named structs with their field names and types (including base classes),
  * enums (top level and nested) with their enumerator names,
  * the public free function signatures in namespace acsd,
  * pure virtual method signatures of the provider/action interfaces,
  * type aliases (Id, U64, Vec2, ArrayRef, HeaderFields, ...).

Only the acsd namespace is dumped (`-ast-dump-filter=acsd`), so the output is
small and stable.  Parsing the AST instead of the C++ text means overloads,
unnamed parameters, nested enums and typedefs are read exactly as the compiler
sees them.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Iterable


# ---------------------------------------------------------------------------
# Type model
# ---------------------------------------------------------------------------

PRIMITIVES = {
    "bool": "bool",
    "double": "f64",
    "float": "f64",
    "std::string": "string",
    "std::uint8_t": "u8",
    "std::uint16_t": "u16",
    "std::uint32_t": "u32",
    "std::uint64_t": "u64",
    "std::int32_t": "i32",
    "std::int64_t": "i64",
    "unsigned int": "u32",
    "unsigned long": "u64",
    "unsigned long long": "u64",
    "int": "i32",
    "long": "i64",
    "long long": "i64",
}


@dataclass(frozen=True)
class Type:
    """A resolved wire type.

    kind is one of: bool, f64, u8, u16, u32, u64, i32, i64, string, id,
    named (a struct), enum, ref (Ref<T>/ArrayRef), vector, array, optional,
    variant, span, unsupported.
    """

    kind: str
    name: str = ""          # qualified C++ name for named/enum, prim spelling
    args: tuple = ()        # nested Type values
    count: int = 0          # std::array length
    reason: str = ""        # for unsupported

    def is_alias(self) -> bool:
        """Refs and spans travel as object-table aliases, never as JSON data."""
        return self.kind == "ref"

    def describe(self) -> str:
        if self.kind in ("named", "enum"):
            return self.name
        if self.kind == "ref":
            return f"Ref<{self.args[0].describe()}>"
        if self.kind == "vector":
            return f"std::vector<{self.args[0].describe()}>"
        if self.kind == "array":
            return f"std::array<{self.args[0].describe()}, {self.count}>"
        if self.kind == "optional":
            return f"std::optional<{self.args[0].describe()}>"
        if self.kind == "variant":
            return "std::variant<" + ", ".join(a.describe() for a in self.args) + ">"
        if self.kind == "span":
            return f"std::span<{self.args[0].describe()}>"
        if self.kind == "unsupported":
            return f"<unsupported {self.name}: {self.reason}>"
        return {"bool": "bool", "f64": "double", "u8": "std::uint8_t",
                "u16": "std::uint16_t", "u32": "U32", "u64": "U64",
                "i32": "std::int32_t", "i64": "std::int64_t",
                "string": "std::string", "id": "Id"}[self.kind]


@dataclass
class Field:
    name: str
    type: Type
    offset: int = 0


@dataclass
class Record:
    name: str                                  # unqualified name
    qualified: str                             # namespace-free C++ name (Nested::Name)
    fields: list[Field] = field(default_factory=list)
    enums: dict[str, list[str]] = field(default_factory=dict)
    methods: list["Method"] = field(default_factory=list)
    bases: list[str] = field(default_factory=list)
    is_abstract: bool = False
    has_virtual: bool = False


@dataclass
class Method:
    name: str
    params: list[tuple[str, str]]              # (name, qualType)
    return_type: str
    is_virtual: bool = False
    is_pure: bool = False
    is_const: bool = False


@dataclass
class Function:
    name: str
    params: list[tuple[str, str]]
    return_type: str
    mangled: str = ""


# ---------------------------------------------------------------------------
# AST dump
# ---------------------------------------------------------------------------

def dump_ast(model_header: Path, clang: str, cache_dir: Path,
             extra_args: Iterable[str] = ()) -> dict:
    """Returns the parsed `acsd` NamespaceDecl of the header."""
    source = model_header.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    cache_dir.mkdir(parents=True, exist_ok=True)
    cache = cache_dir / f"model-{digest[:16]}.ast.json"
    if cache.is_file():
        return json.loads(cache.read_text(encoding="utf-8"))
    command = [
        clang, "-std=c++20", "-fsyntax-only", "-Xclang", "-ast-dump=json",
        "-Xclang", "-ast-dump-filter=acsd", *extra_args, str(model_header),
    ]
    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        raise SystemExit(
            "clang AST dump failed for " + str(model_header) + ":\n" +
            completed.stderr[-4000:])
    text = completed.stdout
    if not text.strip():
        raise SystemExit("clang AST dump produced no output for " +
                         str(model_header))
    parsed = json.loads(text)
    cache.write_text(text, encoding="utf-8")
    return parsed


# ---------------------------------------------------------------------------
# Type string parsing
# ---------------------------------------------------------------------------

def split_template(text: str) -> list[str]:
    """Splits `A, B<C, D>, E` at top-level commas."""
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    for character in text:
        if character == "<":
            depth += 1
        elif character == ">":
            depth -= 1
        if character == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
            continue
        current.append(character)
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def inner_template(text: str) -> str:
    start = text.index("<")
    end = text.rindex(">")
    return text[start + 1:end].strip()


class DeclarationModel:
    def __init__(self, ast: dict):
        self.aliases: dict[str, str] = {}
        self.records: dict[str, Record] = {}
        self.enums: dict[str, list[str]] = {}
        self.functions: dict[str, list[Function]] = {}
        # Names are collected before any field type is resolved: a struct may
        # refer to a type declared later in the header (ActionRecipe ->
        # ErrorRecord), so resolution must not depend on declaration order.
        self.declared_records: set[str] = set()
        self.declared_enums: set[str] = set()
        self._collect_names(ast, prefix="")
        self._parse(ast)

    def _collect_names(self, node: dict, prefix: str) -> None:
        for declaration in node.get("inner", []):
            kind = declaration["kind"]
            if kind == "CXXRecordDecl" and declaration.get("name"):
                qualified = prefix + declaration["name"]
                if declaration.get("completeDefinition") or not declaration.get("isImplicit"):
                    self.declared_records.add(qualified)
                self._collect_names(declaration, qualified + "::")
            elif kind == "EnumDecl" and declaration.get("name"):
                self.declared_enums.add(prefix + declaration["name"])

    # -- construction -----------------------------------------------------

    def _parse(self, ast: dict) -> None:
        if ast.get("kind") != "NamespaceDecl":
            raise SystemExit("expected the acsd NamespaceDecl, got " +
                             ast.get("kind", "?"))
        for declaration in ast.get("inner", []):
            kind = declaration["kind"]
            if kind == "TypeAliasDecl":
                self.aliases[declaration["name"]] = \
                    declaration["type"]["qualType"]
            elif kind == "TypeAliasTemplateDecl":
                self._parse_alias_template(declaration)
            elif kind == "EnumDecl":
                self.enums[declaration["name"]] = self._enumerators(declaration)
            elif kind == "CXXRecordDecl":
                self._parse_record(declaration, prefix="")
            elif kind == "FunctionDecl":
                signature = Function(
                    name=declaration["name"],
                    params=self._params(declaration),
                    return_type=declaration["type"]["qualType"],
                    mangled=declaration.get("mangledName", ""))
                self.functions.setdefault(signature.name, []).append(signature)

    def _parse_alias_template(self, declaration: dict) -> None:
        # template<class T> using Ref = std::shared_ptr<const T>;
        target = declaration["inner"][0]
        if target["kind"] == "TypeAliasDecl":
            self.aliases[declaration["name"]] = target["type"]["qualType"]

    def _parse_record(self, declaration: dict, prefix: str) -> None:
        if declaration.get("isImplicit"):
            return
        name = declaration.get("name")
        if not name:
            return
        qualified = prefix + name
        if not declaration.get("completeDefinition"):
            return
        record = Record(
            name=name, qualified=qualified,
            bases=[base["type"]["qualType"]
                   for base in declaration.get("bases", [])],
            is_abstract=declaration.get("definitionData", {})
                               .get("isAbstract", False),
        )
        for member in declaration.get("inner", []):
            kind = member["kind"]
            if kind == "FieldDecl" and not member.get("isImplicit"):
                record.fields.append(Field(
                    name=member["name"],
                    type=self.parse(member["type"]["qualType"], qualified),
                    offset=member["loc"].get("offset", 0)))
            elif kind == "EnumDecl":
                enum_name = qualified + "::" + member["name"]
                constants = self._enumerators(member)
                self.enums[enum_name] = constants
                record.enums[member["name"]] = constants
            elif kind == "CXXMethodDecl":
                method = Method(
                    name=member["name"],
                    params=self._params(member),
                    return_type=member["type"]["qualType"],
                    is_virtual=member.get("virtual", False),
                    is_pure=member.get("pure", False),
                    is_const=member.get("const", False),
                )
                if member.get("virtual"):
                    record.has_virtual = True
                if member.get("name") != "operator=":
                    record.methods.append(method)
        self.records[qualified] = record
        # Nested record/enum declarations also need a qualified entry; the
        # compiler emits them inside the record, and nested records with their
        # own fields are registered as <Outer>::<Inner>.
        for member in declaration.get("inner", []):
            if member["kind"] == "CXXRecordDecl" and not member.get("isImplicit"):
                self._parse_record(member, prefix=qualified + "::")

    @staticmethod
    def _params(declaration: dict) -> list[tuple[str, str]]:
        params = []
        for member in declaration.get("inner", []):
            if member["kind"] != "ParmVarDecl":
                continue
            params.append((member.get("name") or "",
                           member["type"]["qualType"]))
        return params

    @staticmethod
    def _enumerators(declaration: dict) -> list[str]:
        return [member["name"] for member in declaration.get("inner", [])
                if member["kind"] == "EnumConstantDecl"]

    # -- lookup -----------------------------------------------------------

    def enum_constants(self, qualified: str) -> list[str]:
        constants = self.enums.get(qualified)
        if constants is None:
            raise SystemExit(f"unknown enum '{qualified}'")
        return constants

    def record(self, qualified: str) -> Record:
        record = self.records.get(qualified)
        if record is None:
            raise SystemExit(f"unknown struct '{qualified}'")
        return record

    def resolve_name(self, spelling: str, scope: str | None) -> str:
        """Maps a type spelling to a key in records/enums."""
        spelling = spelling.strip()
        if spelling.startswith("enum "):
            spelling = spelling[len("enum "):].strip()
            if "::" in spelling:
                return spelling
            if scope is not None:
                candidate = scope + "::" + spelling
                if candidate in self.declared_enums:
                    return candidate
            if spelling in self.declared_enums:
                return spelling
            raise SystemExit(f"cannot resolve enum '{spelling}' in {scope}")
        if "::" in spelling:
            return spelling
        if scope is not None:
            candidate = scope + "::" + spelling
            if candidate in self.declared_enums:
                return candidate
        if spelling in self.declared_enums:
            return spelling
        if spelling in self.declared_records:
            return spelling
        raise SystemExit(f"cannot resolve name '{spelling}' in {scope}")

    # -- type parsing -----------------------------------------------------

    def parse(self, spelling: str, scope: str | None) -> Type:
        text = " ".join(spelling.split())
        while text.startswith("const "):
            text = text[len("const "):].strip()
        if text.endswith("&"):
            # A reference parameter/field: the wire type is the value itself.
            # Only const references appear in the public model.
            return self.parse(text[:-1].strip(), scope)
        if text.endswith("*"):
            return Type("unsupported", name=text, reason="raw pointer")
        if text in PRIMITIVES:
            return Type(PRIMITIVES[text])
        if text == "Id":
            return Type("id")
        if text == "ArrayRef":
            return Type("ref", "ArrayRef", (Type("named", "Array"),))
        if text.startswith("Ref<"):
            target = self.parse_pointee(inner_template(text), scope)
            return Type("ref", text, (target,))
        if text.startswith("std::shared_ptr<"):
            target_text = inner_template(text)
            if not target_text.startswith("const "):
                return Type("unsupported", name=text,
                            reason="non-const shared_ptr")
            return Type("ref", text, (self.parse(target_text, scope),))
        if text.startswith("std::span<"):
            return Type("span", text,
                        (self.parse(inner_template(text), scope),))
        if text.startswith("std::vector<"):
            return Type("vector", text,
                        (self.parse(inner_template(text), scope),))
        if text.startswith("std::optional<"):
            return Type("optional", text,
                        (self.parse(inner_template(text), scope),))
        if text.startswith("std::array<"):
            arguments = split_template(inner_template(text))
            if len(arguments) != 2:
                return Type("unsupported", name=text, reason="array arguments")
            return Type("array", text, (self.parse(arguments[0], scope),),
                        count=int(arguments[1]))
        if text.startswith("std::variant<"):
            return Type("variant", text,
                        tuple(self.parse(argument, scope)
                              for argument in split_template(inner_template(text))))
        if text in self.aliases:
            return self.parse(self.aliases[text], scope)
        if text.startswith("enum ") or text in self.declared_enums or "::" in text:
            return Type("enum", self.resolve_name(text, scope))
        if text in self.declared_records:
            record = self.records.get(text)
            if record is not None and record.has_virtual:
                return Type("unsupported", name=text,
                            reason="polymorphic type is only wireable through "
                                   "a Ref object alias")
            return Type("named", text)
        return Type("unsupported", name=text, reason="unrecognised type")

    def parse_pointee(self, spelling: str, scope: str | None) -> Type:
        """Parses the T of Ref<T>.

        A Ref may point at a polymorphic interface (LinearAction,
        FramesProvider, ...) that is never materialised from JSON; the ref
        itself is a valid wire type (an object-table alias), so the pointee
        only needs a name, not a codec.
        """
        text = " ".join(spelling.split())
        while text.startswith("const "):
            text = text[len("const "):].strip()
        if text in self.aliases:
            return self.parse_pointee(self.aliases[text], scope)
        if text == "ArrayRef":
            return Type("named", "Array")
        if text in self.declared_records:
            return Type("named", text)
        if text in self.declared_enums:
            return Type("enum", text)
        return self.parse(text, scope)
