"""集成层测试的共用合同面：最小 JSON-Schema 校验器 + 统一对象合同解析。

本模块**不含任何测试函数**，只提供被`test_variance_propagation.py` 调用的读取、
校验与判定原语。它是本层的工具面，不是判据面。

## 为什么自带校验器

`jsonschema` 包在本机不可用，而`eng/tests/schemas/unified/` 下 14 个统一对象
schema 是本层 A1/A2 判据的被测对象（判据见
`eng/tests/integration/test_variance_propagation.py`）。因此本模块用**纯标准库**
实现一个 JSON-Schema 子集校验器。

**自研校验器必须被独立判据约束**（见 `VALIDATION_EVIDENCE.md` 第 8 节 S6「内容级
负例」）：`test_ContractVariance_ValidatorSelfCheckNonVacuous` 证明它确实在做功，
`test_ContractVariance_NegVacuousValidator` 证明把它弄空转就会判红。

## 关键字面（实测非推测）

`eng/contracts/schemas/unified/*.schema.json` 实测用到的断言性关键字只有：
`type` `enum` `const` `required` `properties` `additionalProperties`
`propertyNames` `pattern` `minLength` `maxLength` `minItems` `maxItems`
`uniqueItems` `items` `minimum` `maximum` `exclusiveMinimum`
`exclusiveMaximum` `allOf` `anyOf` `oneOf` `not` `if` `then` `else`。
未用到 `$ref` / `definitions` / `$defs` / `format` / `multipleOf`。

**方言说明（如实登记）**：这 14 个文件的头部写的是
`"$schema": "https://json-schema.org/draft/2020-12/schema"`（例如
`signal.schema.json:2`），而集成层派单书里说的是 draft-07。对上面这个关键字子集，
两个方言的语义**逐项一致**（`exclusiveMinimum` 在两版都是数值型，`items` 只用数组
形式），所以本校验器同时对两版成立。

**未知关键字一律 fail-closed**：`SUPPORTED_KEYWORDS` 之外的键被当作不支持关键字
报成一条错误，而不是静默忽略——静默忽略会把「schema 写了本层没实现的约束」读成
「通过」，属`VALIDATION_EVIDENCE.md` 第 12.3 节禁止的「读不到就按全部合规处理」。

**`x-` 扩展面必须显式放行，不接受「前缀即注记」**（对抗复核修正）。
早期版本把任何 `x-*` 当注记无条件跳过，实测
`validate("x", {"type": "string", "x-must-be-positive": True})` 判 `ok=True`
—— 约束只要藏进 `x-` 键就静默失效，是一个真实的 fail-open。
现在只有 `ANNOTATION_PREFIXES` 白名单里的前缀被放行（本仓实测只用到
`x-acsd`、`x-acsd-gate`、`x-acsd-object`），其余 `x-*` 一律报
`UnsupportedKeyword`。负例 `test_ContractVariance_NegUnknownXKeywordFailOpen` 盯这条。

**数组形 `items`（draft-07 的 tuple 形式）不实现，且必须具名报错**（对抗复核修正）。
实测 `validate(["a", 5], {"items": [{"type": "string"}, {"type": "integer"}]})`
在早期版本里返回 `ok=True`（数组形被当成「不是 dict ⇒ 不适用」而跳过）——
同类的 fail-open。现在数组形 `items` 报 `UnsupportedKeyword`。
当前 14 个 canonical schema 全部只用对象形 `items`，故不丢覆盖面。
负例 `test_ContractVariance_NegArrayFormItemsIgnored` 盯这条。

## 错误位置编码

每条错误带三个可归因字段（`VALIDATION_EVIDENCE.md` 第 6 节 Q3）：

| 字段 | 含义 |
|---|---|
| `location` | 相对实例根的路径，段间 `/` 分隔，顶层为 `""` |
| `pointer` | 同上的 RFC 6901 形式，顶层为 `""`，其余以 `/` 开头 |
| `keyword` | 触发判定的关键字 |

`EXPECTED.json` 的 `must_match` 读法由 `error_matches` 实现：带 `:关键字` 的条目
要求「位置 + 关键字」都精确相等；不带冒号的条目要求「该位置或该位置以下的任意
错误」或「该位置缺少这个 required 键」命中。两种读法都登记在
`eng/tests/integration/README.md`。
"""

from __future__ import annotations

import json
import math
import re
from pathlib import Path
from typing import Any

# --------------------------------------------------------------------------
# 仓库锚（VALIDATION_EVIDENCE.md 第 12.3 节「锚存活」：路径缺失具名判红）
# --------------------------------------------------------------------------

#: 仓库根。eng/tests/integration/_object_contracts.py → 上溯三级。
REPO_ROOT = Path(__file__).resolve().parents[3]

#: 统一对象 schema 面。
SCHEMA_DIR = REPO_ROOT / "eng" / "contracts" / "schemas" / "unified"

#: 正例夹具面。
EXAMPLE_DIR = SCHEMA_DIR / "examples"

#: 负例夹具面。
NEGATIVE_DIR = SCHEMA_DIR / "negative"

#: 统一对象登记表。
REGISTRY_PATH = REPO_ROOT / "eng" / "contracts" / "data" / "unified_object_registry.json"

#: 条款登记表。
CLAUSE_REGISTRY_PATH = REPO_ROOT / "eng" / "contracts" / "data" / "clause_registry.json"


class AnchorStale(RuntimeError):
    """仓库锚缺失/不可解析。调用方必须把它转成具名判红，不得静默降级。"""


def require(path: Path, what: str) -> Path:
    """锚存活检查：路径不存在即抛 `AnchorStale`（fail-closed）。"""
    if not path.exists():
        raise AnchorStale(f"ANCHOR_STALE: {what} {path}")
    return path


def load_json(path: Path, what: str) -> Any:
    """读 JSON。解析失败是硬失败，不降级、不跳过。"""
    require(path, what)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except json.JSONDecodeError as exc:  # pragma: no cover - 只在锚损坏时触发
        raise AnchorStale(f"ANCHOR_UNPARSABLE: {what} {path}: {exc}") from exc


def schema_path(object_name: str) -> Path:
    return SCHEMA_DIR / f"{object_name}.schema.json"


def load_schema(object_name: str) -> dict:
    return load_json(schema_path(object_name), f"schema:{object_name}")


def load_example(file_name: str) -> dict:
    return load_json(EXAMPLE_DIR / file_name, f"example:{file_name}")


def load_negative(file_name: str) -> dict:
    return load_json(NEGATIVE_DIR / file_name, f"negative:{file_name}")


def load_negative_expectations() -> dict:
    return load_json(NEGATIVE_DIR / "EXPECTED.json", "negative:EXPECTED.json")


def all_example_files() -> list[Path]:
    require(EXAMPLE_DIR, "example-dir")
    return sorted(EXAMPLE_DIR.glob("*.example.json"))


def all_schema_files() -> list[Path]:
    require(SCHEMA_DIR, "schema-dir")
    return sorted(SCHEMA_DIR.glob("*.schema.json"))


# --------------------------------------------------------------------------
# TEST.md 第 4 节冻结容差（逐字抄录，正本 = docs/engineering/testing/TEST.md）
# --------------------------------------------------------------------------

#: TEST.md:46「元数据、掩膜、计数、索引、端口、选择结果 | 精确一致」。
TOLERANCE_EXACT = "exact"

#: TEST.md:47「双精度非归约 | rtol = 1e-12，atol = 1e-13 × scale」。
RTOL_F64 = 1e-12
ATOL_F64_SCALE_FACTOR = 1e-13

#: TEST.md:48「单精度产品非归约 | rtol = 5e-6，atol = 1e-6 × scale」。
RTOL_F32 = 5e-6
ATOL_F32_SCALE_FACTOR = 1e-6

#: TEST.md:49「归约 | γ_n = n·u/(1−n·u)，门限 C·γ_n·Σ|terms| + atol，C ≤ 4 事前冻结」。
REDUCTION_AMPLIFICATION_C = 4  # TEST.md:49 「C ≤ 4 事前冻结」；取上界，判据不放松
#: TEST.md:58「IEEE 754 binary64 | 2⁻⁵³ | 1.1102230246251565e-16」。
UNIT_ROUNDOFF_F64 = 1.1102230246251565e-16
#: TEST.md:59「IEEE 754 binary32 | 2⁻²⁴ | 5.9604644775390625e-08」。
UNIT_ROUNDOFF_F32 = 5.9604644775390625e-08


def gamma_n(n_terms: int, u: float = UNIT_ROUNDOFF_F64) -> float:
    """归约相对误差因子 γ_n = n·u/(1−n·u)（TEST.md:49、:62）。

    `n_terms` 必须是**该次归约的实际项数**，不是数据总像素数（TEST.md:62）。
    """
    if n_terms < 1:
        raise ValueError(f"n_terms 必须 ≥ 1，实际 {n_terms}")
    return (n_terms * u) / (1.0 - n_terms * u)


def reduction_threshold(n_terms: int, sum_abs_terms: float, scale: float,
                        u: float = UNIT_ROUNDOFF_F64,
                        c: float = REDUCTION_AMPLIFICATION_C) -> float:
    """归约档门限 C·γ_n·Σ|terms| + atol（TEST.md:49 逐字）。

    `atol` 取 TEST.md:47 的双精度档 `1e-13 × scale`。
    """
    return c * gamma_n(n_terms, u) * abs(sum_abs_terms) + ATOL_F64_SCALE_FACTOR * abs(scale)


def ulp(scale: float) -> float:
    """1 ulp(scale)：TEST.md:73 的绝对容差可满足性下限。"""
    if scale == 0.0:
        return float.fromhex("0x1.0p-1074")  # 最小次正规数
    return float(math.nextafter(abs(scale), math.inf)) - abs(scale)


def atol_is_decidable(scale: float, atol: float | None = None) -> bool:
    """TEST.md:73：绝对容差只有在 atol ≥ 1 ulp(scale) 时才可判。"""
    if atol is None:
        atol = ATOL_F64_SCALE_FACTOR * abs(scale)
    return atol >= ulp(scale)


# --------------------------------------------------------------------------
# JSON-Schema 子集校验器
# --------------------------------------------------------------------------

#: 断言性关键字（真会影响判定）。
SUPPORTED_KEYWORDS = frozenset(
    {
        "type", "enum", "const",
        "required", "properties", "additionalProperties", "propertyNames",
        "pattern", "minLength", "maxLength",
        "minItems", "maxItems", "items", "uniqueItems",
        "minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum",
        "allOf", "anyOf", "oneOf", "not", "if", "then", "else",
    }
)

#: 注记关键字（不影响判定，允许出现）。
ANNOTATION_KEYWORDS = frozenset(
    {"$schema", "$id", "title", "description", "default", "examples", "$comment"}
)

#: 本仓 `x-` 扩展注记面的**白名单前缀**。
#:
#: 为什么不能「任何 `x-` 前缀都当注记」：对抗复核实测
#: `validate("x", {"type": "string", "x-must-be-positive": True})` 判 `ok=True`
#: —— 约束藏进 `x-` 键即静默失效，是真实的 fail-open。
#: 实测本仓 14 个 canonical schema 只用到 `x-acsd`、`x-acsd-gate`、
#: `x-acsd-object`，故白名单取 `x-acsd` 前缀。
ANNOTATION_PREFIXES = ("x-acsd",)


def _is_annotation(key: str) -> bool:
    """判断一个 schema 位置的键是否允许作为**注记**出现。

    只有两种情形放行：标准注记关键字，或命中 `ANNOTATION_PREFIXES` 白名单的
    `x-` 扩展键。其余一律不当注记 → 由调用方报 `UnsupportedKeyword`。
    """
    if key in ANNOTATION_KEYWORDS:
        return True
    if key.startswith("x-"):
        return any(key == p or key.startswith(p + "-") for p in ANNOTATION_PREFIXES)
    return False


def join(path: str, key: str) -> str:
    return f"{path}/{key}" if path else key


def _type_ok(instance: Any, type_name: str) -> bool:
    if type_name == "object":
        return isinstance(instance, dict)
    if type_name == "array":
        return isinstance(instance, list)
    if type_name == "string":
        return isinstance(instance, str)
    if type_name == "boolean":
        return isinstance(instance, bool)
    if type_name == "null":
        return instance is None
    if type_name == "number":
        return isinstance(instance, (int, float)) and not isinstance(instance, bool)
    if type_name == "integer":
        if isinstance(instance, bool):
            return False
        if isinstance(instance, int):
            return True
        return isinstance(instance, float) and instance.is_integer()
    raise ValueError(f"未知 type: {type_name}")


def _canonical(value: Any) -> Any:
    """JSON 规范化：把 1 与 1.0 视作同一个数（JSON Schema 的 const/enum 语义）。"""
    if isinstance(value, bool):
        return ("bool", value)
    if isinstance(value, (int, float)):
        return ("num", float(value))
    if isinstance(value, dict):
        return ("obj", tuple(sorted((k, _canonical(v)) for k, v in value.items())))
    if isinstance(value, list):
        return ("arr", tuple(_canonical(v) for v in value))
    if value is None:
        return ("null",)
    return (type(value).__name__, value)


def json_equal(left: Any, right: Any) -> bool:
    return _canonical(left) == _canonical(right)


def validate(
    instance: Any,
    schema: Any,
    *,
    location: str = "",
    skip_required: bool = False,
) -> tuple[bool, list[dict]]:
    """校验 `instance` 是否满足 `schema` 子集。

    返回 `(ok, errors)`。`errors` 每条含 `location` / `pointer` / `keyword` /
    `detail`，`required` 违规另带 `missing`（缺哪个键）。

    `skip_required=True` 是**唯一的可注入失效开关**：B7 负例用它把 `required`
    分支摘掉，用来证明本校验器不是空转的。生产路径一律用默认 False。
    """
    errors: list[dict] = []

    def emit(keyword: str, loc: str, detail: str, sink: list[dict],
             missing: str | None = None) -> None:
        record = {
            "location": loc,
            "pointer": ("/" + loc) if loc else "",
            "keyword": keyword,
            "detail": detail,
        }
        if missing is not None:
            record["missing"] = missing
        sink.append(record)

    def walk(inst: Any, sch: Any, path: str, sink: list[dict]) -> bool:
        """`sink` 是本子模式的错误收集器。

        `if` / `not` / 已成立的 `anyOf` 分支的错误**不得**进主 `errors`：
        那些子模式的成立与否本身就是判定，条件为假时它报告的内容不是违规。
        把它们混进来会让「正例通过」被无关错误淹没（属`VALIDATION_EVIDENCE.md`
        第 12.6 节的假红面）。
        """
        if not isinstance(sch, dict):
            emit("SchemaShape", path, f"schema 位置不是对象: {type(sch).__name__}", sink=sink)
            return False

        # --- 未知关键字 fail-closed：静默忽略会把「没实现的约束」读成「通过」 ---
        unknown = [k for k in sch if k not in SUPPORTED_KEYWORDS and not _is_annotation(k)]
        if unknown:
            for key in sorted(unknown):
                emit("UnsupportedKeyword", path, f"本层未实现关键字 {key!r}", sink=sink)
            return False

        ok = True

        if "const" in sch and not json_equal(inst, sch["const"]):
            emit("const", path, f"const={sch['const']!r}", sink=sink)
            ok = False

        if "enum" in sch and not any(json_equal(inst, item) for item in sch["enum"]):
            emit("enum", path, f"enum={sch['enum']!r}", sink=sink)
            ok = False

        if "type" in sch:
            names = sch["type"] if isinstance(sch["type"], list) else [sch["type"]]
            if not any(_type_ok(inst, name) for name in names):
                emit("type", path, f"type={sch['type']!r}", sink=sink)
                return False  # 类型不符时后续关键字语义无意义

        # --- 字符串面 ---
        if isinstance(inst, str):
            if "minLength" in sch and len(inst) < sch["minLength"]:
                emit("minLength", path, f"len={len(inst)} < {sch['minLength']}", sink=sink)
                ok = False
            if "maxLength" in sch and len(inst) > sch["maxLength"]:
                emit("maxLength", path, f"len={len(inst)} > {sch['maxLength']}", sink=sink)
                ok = False
            if "pattern" in sch and re.search(sch["pattern"], inst) is None:
                emit("pattern", path, f"pattern={sch['pattern']!r}", sink=sink)
                ok = False

        # --- 数值面（JSON Schema 的 number/integer 不分 bool）---
        if isinstance(inst, (int, float)) and not isinstance(inst, bool):
            if "minimum" in sch and inst < sch["minimum"]:
                emit("minimum", path, f"{inst!r} < {sch['minimum']!r}", sink=sink)
                ok = False
            if "maximum" in sch and inst > sch["maximum"]:
                emit("maximum", path, f"{inst!r} > {sch['maximum']!r}", sink=sink)
                ok = False
            if "exclusiveMinimum" in sch and not inst > sch["exclusiveMinimum"]:
                emit("exclusiveMinimum", path, f"{inst!r} <= {sch['exclusiveMinimum']!r}", sink=sink)
                ok = False
            if "exclusiveMaximum" in sch and not inst < sch["exclusiveMaximum"]:
                emit("exclusiveMaximum", path, f"{inst!r} >= {sch['exclusiveMaximum']!r}", sink=sink)
                ok = False

        # --- 数组面 ---
        if isinstance(inst, list):
            if "minItems" in sch and len(inst) < sch["minItems"]:
                emit("minItems", path, f"len={len(inst)} < {sch['minItems']}", sink=sink)
                ok = False
            if "maxItems" in sch and len(inst) > sch["maxItems"]:
                emit("maxItems", path, f"len={len(inst)} > {sch['maxItems']}", sink=sink)
                ok = False
            if sch.get("uniqueItems") is True:
                for i in range(len(inst)):
                    for j in range(i + 1, len(inst)):
                        if json_equal(inst[i], inst[j]):
                            emit("uniqueItems", path, f"下标 {i} 与 {j} 重复", sink=sink)
                            ok = False
                            break
            if isinstance(sch.get("items"), dict):
                for index, item in enumerate(inst):
                    if not walk(item, sch["items"], join(path, str(index)), sink):
                        ok = False
            elif isinstance(sch.get("items"), list):
                # 数组形 `items`（draft-07 的 tuple 形式）**未实现**。必须具名报错，
                # 不能因为「不是 dict」就跳过——跳过等于把「本层没实现这个约束」
                # 读成「实例满足它」，是 fail-open（对抗复核实证）。
                emit("UnsupportedKeyword", path,
                     "数组形 items（draft-07 tuple 形式）本层未实现："
                     "不按逐位校验也不放行", sink=sink)
                ok = False

        # --- 对象面 ---
        if isinstance(inst, dict):
            properties = sch.get("properties", {})
            if not skip_required:
                for name in sch.get("required", []):
                    if name not in inst:
                        emit("required", path, f"缺必填键 {name!r}", sink=sink, missing=name)
                        ok = False

            name_schema = sch.get("propertyNames")
            if isinstance(name_schema, dict):
                for key in inst:
                    # 键名违规定位在**键本身**，与 additionalProperties 同址，
                    # 这样 EXPECTED.json 的 `weight:pattern` / `weight:additionalProperties`
                    # 两条都落在 `weight` 上。
                    if not walk(key, name_schema, join(path, key), sink):
                        ok = False

            for key, value in inst.items():
                if key in properties:
                    if not walk(value, properties[key], join(path, key), sink):
                        ok = False

            additional = sch.get("additionalProperties")
            if additional is False:
                for key in inst:
                    if key not in properties:
                        emit("additionalProperties", join(path, key), f"未声明键 {key!r}", sink=sink)
                        ok = False
            elif isinstance(additional, dict):
                for key, value in inst.items():
                    if key not in properties:
                        if not walk(value, additional, join(path, key), sink):
                            ok = False

        # --- 组合面 ---
        for keyword in ("allOf", "anyOf", "oneOf"):
            branches = sch.get(keyword)
            if not isinstance(branches, list):
                continue
            if keyword == "allOf":
                # allOf 的每条分支都必须成立 ⇒ 分支错误直接进主 sink。
                results = [walk(inst, branch, path, sink) for branch in branches]
                if not all(results):
                    ok = False
                continue

            branch_errors: list[list[dict]] = [[] for _ in branches]
            results = [walk(inst, branch, path, bucket) for branch, bucket in zip(branches, branch_errors)]
            if keyword == "anyOf":
                if not any(results):
                    ok = False
                    emit("anyOf", path, "anyOf 全部分支不成立", sink=sink)
                    # 全败时各分支的违规本身也是证据，一并落盘便于定位。
                    for bucket in branch_errors:
                        sink.extend(bucket)
            else:  # oneOf
                hits = sum(1 for r in results if r)
                if hits != 1:
                    emit("oneOf", path, f"命中分支数={hits}（须恰为 1）", sink=sink)
                    ok = False

        if "not" in sch:
            probe: list[dict] = []
            if walk(inst, sch["not"], path, probe):
                emit("not", path, "not 子模式成立", sink=sink)
                ok = False

        if "if" in sch:
            probe = []
            condition_ok = walk(inst, sch["if"], path, probe)
            branch = "then" if condition_ok else "else"
            if isinstance(sch.get(branch), dict):
                if not walk(inst, sch[branch], path, sink):
                    ok = False

        return ok

    return walk(instance, schema, location, errors), errors


def error_matches(error: dict, spec: str) -> bool:
    """`EXPECTED.json` 的 `must_match` 条目读法。

    - `位置:关键字` ⇒ 位置与关键字都精确相等；
    - `位置` ⇒ 命中「该位置上的错误」「该位置以下的错误」或「该位置缺这个
      required 键」三者之一。
    """
    if ":" in spec:
        head, tail = spec.rsplit(":", 1)
        if tail not in SUPPORTED_KEYWORDS:
            return False
        return error.get("location") == head and error.get("keyword") == tail
    loc = error.get("location", "")
    return loc == spec or loc.startswith(spec + "/") or error.get("missing") == spec


def hit_expectations(errors: list[dict], must_match: list[str]) -> tuple[list[str], list[str]]:
    """返回 `(命中条目, 未命中条目)`。"""
    hit = [spec for spec in must_match if any(error_matches(e, spec) for e in errors)]
    miss = [spec for spec in must_match if spec not in hit]
    return hit, miss


def format_errors(errors: list[dict], limit: int = 10) -> str:
    lines = []
    for err in errors[:limit]:
        missing = f" missing={err['missing']!r}" if "missing" in err else ""
        lines.append(f"{err['pointer'] or '/'}[{err['keyword']}]{missing} {err['detail']}")
    if len(errors) > limit:
        lines.append(f"... 另有 {len(errors) - limit} 条")
    return "\n".join(lines)


# --------------------------------------------------------------------------
# 统一对象合同解析（不含判定，只做解析与取值）
# --------------------------------------------------------------------------

#: 方差链上「像素方差 / 像素逆方差 / 帧级信噪比 / 稀疏层信噪比」四环的对象名。
VARIANCE_CHAIN_OBJECTS = ("signal", "ivar", "variance", "frame_snr", "sparse_snr_layer")


def object_identity(document: Any) -> tuple[str, str]:
    """取出 `(unified_object, object_schema_id)`。缺键即抛错（调用方转判红）。"""
    if not isinstance(document, dict):
        raise AnchorStale(f"不是对象文档: {type(document).__name__}")
    for key in ("unified_object", "object_schema_id"):
        if key not in document:
            raise AnchorStale(f"对象文档缺判别键 {key!r}")
    return str(document["unified_object"]), str(document["object_schema_id"])


def schema_declared_identity(schema: dict) -> tuple[str, str]:
    """从 schema 取出 `(对象名, $id)`；两者必须与实例文档逐字一致。"""
    try:
        return str(schema["properties"]["unified_object"]["const"]), str(schema["$id"])
    except (KeyError, TypeError) as exc:
        raise AnchorStale(f"schema 缺对象判别式或 $id: {exc}") from exc


def units_of(document: dict) -> dict:
    units = document.get("units")
    if not isinstance(units, dict):
        raise AnchorStale("对象文档缺 units 对象")
    return units


def missing_value_of(document: dict) -> dict:
    block = document.get("missing_value")
    if not isinstance(block, dict):
        raise AnchorStale("对象文档缺 missing_value 对象")
    return block


def gate_clause_ids(schema: dict) -> list[str]:
    """抽出 schema 内 `x-acsd-gate` 标注的条款号（证据可归因到条款）。"""
    found: list[str] = []

    def walk(node: Any) -> None:
        if isinstance(node, dict):
            gate = node.get("x-acsd-gate")
            if isinstance(gate, dict) and "clause_id" in gate:
                clause = str(gate["clause_id"])
                if clause not in found:
                    found.append(clause)
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(schema)
    return found


def retired_object_names(schema: dict) -> list[str]:
    """抽出 `x-acsd-object.retired_objects` 里登记的退役对象名。"""
    header = schema.get("x-acsd-object")
    if not isinstance(header, dict):
        return []
    names: list[str] = []
    for entry in header.get("retired_objects", []) or []:
        if isinstance(entry, dict) and "retired_object" in entry:
            names.append(str(entry["retired_object"]))
    return names


def accepted_object_enum(schema: dict) -> list[str]:
    """抽出端口合同 `accepts_object` 的封闭枚举。"""
    try:
        return list(schema["properties"]["accepts_object"]["enum"])
    except (KeyError, TypeError) as exc:
        raise AnchorStale(f"port_contract 缺 accepts_object 枚举: {exc}") from exc


def accepted_schema_id_enum(schema: dict) -> list[str]:
    """抽出端口合同 `accepts_schema_id` 的封闭枚举。"""
    try:
        return list(schema["properties"]["accepts_schema_id"]["enum"])
    except (KeyError, TypeError) as exc:
        raise AnchorStale(f"port_contract 缺 accepts_schema_id 枚举: {exc}") from exc


def registry_object_names() -> list[str]:
    """统一对象登记表里的 canonical 对象名（13 个）。"""
    registry = load_json(REGISTRY_PATH, "registry:unified_object_registry")
    return [str(entry["object_name"]) for entry in registry["canonical_object_classes"]]