#!/usr/bin/env python3
"""DATA-001 —— 统一数据对象合同链（UNIFIED_MODEL §2 的 13 个对象）契约测试。

覆盖（全部为真实机器门，不是文档约定）：
  A. 13 个对象各一份 canonical schema 落在 eng/contracts/schemas/，schema ID 全局唯一、对象名无二主；
  B. 模糊字段名守卫（weight/value/mask/snr 单独出现即不合格）在 schema 内真实生效；
  C. 五个负例各自必败，且失败原因命中指定门（不是"恰好别处报错"）；
  D. 单位 / BUNIT 语义 / 无效值 / 精度 / 可否作权重 与 UNIFIED_MODEL §2 表格逐字一致；
  E. 三类配置分离锚点：phase_config / cpu_profile / run_manifest 字段名不得共用（配置 schema 本体归 CFG-001）。

运行：python3 -m unittest discover -s eng/tests/contracts -t eng/tests/contracts
      或 python3 eng/tests/contracts/test_unified_object_contract.py
"""
import json
import pathlib
import re
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]

if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

# 校验器按显式仓库路径加载（eng/tests/common/jsonschema_min.py，零第三方依赖），
# 不依赖 unittest 的顶层包布局。
import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "unified_object_jsonschema_min", REPO / "eng/tests/common/jsonschema_min.py")
jm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(jm)

SCHEMAS = REPO / "eng/contracts/schemas"
UNIFIED = SCHEMAS / "unified"
EXAMPLES = UNIFIED / "examples"
NEGATIVE = UNIFIED / "negative"
REGISTRY_REL = "docs/contracts/unified_object_registry.json"

# UNIFIED_MODEL §2 的 13 个对象名（逐字）
OBJECTS = ["signal", "variance", "ivar", "source_snr", "depth_m5", "frame_snr",
           "point_information", "sparse_snr_layer",
           "support", "coverage", "validity", "rejection", "provenance"]

# UNIFIED_MODEL §2 表格「可否作权重」列原文（逐字照抄，不得自行改判定）
VERDICT = {
    "signal": ("否", False),
    "variance": ("对该估计目标可以", True),
    "ivar": ("对该估计目标可以", True),
    "source_snr": ("不直接作帧权重", False),
    "depth_m5": ("摘要，不作权重", False),
    "frame_snr": ("唯一帧级参考；权重由 Phase2 逆方差叠加从 SNR 计算", False),
    "point_information": ("点源目标的严格权重", True),
    "sparse_snr_layer": ("帧内精细参考", False),
    "support": ("否", False),
    "coverage": ("否", False),
    "validity": ("门，不是权重", False),
    "rejection": ("门/概率，不是 coverage", False),
    "provenance": ("——", False),
}

AMBIGUOUS = ("weight", "value", "mask", "snr")


def load(p):
    return json.loads(pathlib.Path(p).read_text(encoding="utf-8"))


def merged_errors(doc, schema):
    return " | ".join("%s:%s" % ("/".join(str(x) for x in p) or "<root>", m)
                      for p, m in jm.validate(doc, schema))


class TestCanonicalObjectSchemas(unittest.TestCase):
    """A. canonical schema 落位、ID 全局唯一、对象无二主。"""

    def test_unified_dir_exists(self):
        self.assertTrue(UNIFIED.is_dir(), "eng/contracts/schemas/unified/ 缺失：canonical 对象合同必须落 eng/contracts/schemas/")
        found = sorted(p.name for p in UNIFIED.glob("*.schema.json"))
        self.assertEqual(sorted(["%s.schema.json" % o for o in OBJECTS] + ["port_contract.schema.json"]), found,
                         "eng/contracts/schemas/unified/ 必须且只能有 13 个对象 schema + 1 个端口合同 schema")

    def test_port_contract_schema_is_canonical_and_loadable(self):
        """负例④指向的 port_contract 必须是真实存在、可被 jsonschema_min 加载的 schema（不是索引里的内联约定）。"""
        reg = load(REPO / REGISTRY_REL)
        ref = reg["port_contract_ref"]
        self.assertEqual("https://astrocs.local/schemas/unified/port/v1", ref["schema_id"])
        self.assertEqual("eng/contracts/schemas/unified/port_contract.schema.json", ref["canonical_schema_file"])
        doc = load(REPO / ref["canonical_schema_file"])
        self.assertEqual(ref["schema_id"], doc["$id"])
        self.assertEqual("object", doc["type"])
        self.assertIn("accepts_object", doc["required"])
        self.assertIn("connected_object_document", doc["required"])
        self.assertEqual(sorted(OBJECTS), sorted(doc["properties"]["accepts_object"]["enum"]))
        self.assertEqual(len(OBJECTS), len(doc["allOf"]), "端口合同必须对每个对象给出 if/then 同对象门")
        # 端口合同必须真能判红：source_snr 文档接进 variance 端口
        neg = load(NEGATIVE / "n4_source_snr_into_variance_port.schema-violation.json")
        errs = merged_errors(neg, doc)
        self.assertIn("connected_object_document/unified_object", errs)
        # 正的连接必须通过
        ok = dict(neg)
        ok["connected_object_document"] = {
            "unified_object": "variance",
            "object_schema_id": "https://astrocs.local/schemas/unified/variance/v1"}
        self.assertEqual("", merged_errors(ok, doc))

    def test_every_schema_loads_and_has_unique_global_id(self):
        ids, loaded = {}, []
        for p in sorted(SCHEMAS.rglob("*.schema.json")):
            doc = load(p)
            loaded.append(p.relative_to(REPO).as_posix())
            sid = doc.get("$id")
            if sid is None:
                continue
            self.assertNotIn(sid, ids, "schema ID 不唯一: %s (同时出现在 %s 与 %s)" % (sid, ids.get(sid), p))
            ids[sid] = p.relative_to(REPO).as_posix()
        # 13 个 canonical 对象 schema 必须全部声明 $id 且互不相同（不得靠别处文件凑数）
        self.assertTrue(all(("https://astrocs.local/schemas/unified/%s/v1" % o) in ids for o in OBJECTS))
        canonical = {}
        for o in OBJECTS:
            sid = "https://astrocs.local/schemas/unified/%s/v1" % o
            self.assertIn(sid, ids, "缺少 canonical schema ID: %s" % sid)
            canonical[sid] = ids[sid]
        self.assertEqual(len(OBJECTS), len(set(canonical.values())),
                         "canonical schema 文件不唯一: %s" % canonical)

    def test_object_name_has_exactly_one_canonical_file(self):
        owner = {}
        for p in sorted(UNIFIED.glob("*.schema.json")):
            if p.name == "port_contract.schema.json":
                continue  # 端口合同不是数据对象

            doc = load(p)
            name = doc["properties"]["unified_object"]["const"]
            self.assertEqual(p.stem.replace(".schema", ""), name, "文件名与对象名不一致: %s" % p)
            self.assertNotIn(name, owner, "对象 %s 有两个 canonical 文件: %s / %s" % (name, owner.get(name), p))
            owner[name] = p.relative_to(REPO).as_posix()
            self.assertEqual(name, doc["x-astrocs-object"]["name"])
        self.assertEqual(sorted(OBJECTS), sorted(owner))

    def test_every_object_schema_is_in_the_ownership_registry(self):
        reg = load(REPO / REGISTRY_REL)
        declared = {c["object_name"]: c for c in reg["canonical_object_classes"]}
        self.assertEqual(sorted(OBJECTS), sorted(declared))
        for name, cls in declared.items():
            doc = load(REPO / cls["canonical_schema_file"])
            self.assertEqual(name, doc["properties"]["unified_object"]["const"])
            self.assertEqual(cls["schema_id"], doc["$id"])
            self.assertEqual("eng/contracts/schemas/unified/%s.schema.json" % name, cls["canonical_schema_file"])
            self.assertTrue((REPO / cls["canonical_schema_file"]).is_file())
        # 每条 eng/contracts/schemas/** 下的 schema 都必须被登记为某个对象类的 canonical / projection / other（U-02 口径）
        owned = set()
        for cls in reg["canonical_object_classes"]:
            owned.add(cls["canonical_schema_file"])
            for proj in cls.get("compatibility_projections", []):
                owned.add(proj["file"])
        for other in reg["other_schema_files"]:
            self.assertTrue((REPO / other["file"]).is_file(), "other_schema_files 路径不存在: %s" % other["file"])
            self.assertTrue(other["owner"], "other_schema_files 缺 owner: %s" % other["file"])
            self.assertTrue(other["note"], "other_schema_files 缺 note: %s" % other["file"])
            self.assertTrue(other["object_class"] is None or other["object_class"] in OBJECTS)
            owned.add(other["file"])
        actual = {p.relative_to(REPO).as_posix() for p in SCHEMAS.rglob("*.schema.json")}
        self.assertEqual(set(), actual - owned, "ownership 未登记的 schema 文件: %s" % sorted(actual - owned))
        cfg_roles = {o["file"]: o for o in reg["other_schema_files"] if o["role"] == "config_contract"}
        for rel in ("eng/contracts/schemas/phase_config_normalize.schema.json",
                    "eng/contracts/schemas/phase_config_mosaic.schema.json",
                    "eng/contracts/schemas/phase_config_export.schema.json",
                    "eng/contracts/schemas/run_manifest.schema.json"):
            self.assertIn(rel, cfg_roles, "%s 必须登记为 CFG-001 的 config_contract" % rel)
            self.assertEqual("CFG-001", cfg_roles[rel]["owner"])
            self.assertIsNone(cfg_roles[rel]["object_class"], "配置 schema 不得被登记为数据对象 canonical")
        self.assertEqual(set(), owned - actual, "ownership 登记的路径不存在: %s" % sorted(owned - actual))

    def test_no_two_equivalent_schemas_per_object(self):
        """U-02 口径的可复跑断言：每个对象恰 1 个 canonical + 0..n 个非等价投影（投影不得重复 canonical 的判定词表）。"""
        reg = load(REPO / REGISTRY_REL)
        for cls in reg["canonical_object_classes"]:
            self.assertEqual(1, sum(1 for c in reg["canonical_object_classes"]
                                     if c["object_name"] == cls["object_name"]))
            canon = load(REPO / cls["canonical_schema_file"])
            canon_kind_fields = {k for k in canon["properties"] if k.endswith("_schema")}
            for proj in cls.get("compatibility_projections", []):
                self.assertNotEqual(cls["canonical_schema_file"], proj["file"])
                other = load(REPO / proj["file"])
                # (i) 投影不得复用 canonical schema ID
                self.assertNotEqual(canon["$id"], other["$id"], "投影不得复用 canonical schema ID")
                # (ii) 投影不得重复 canonical 的对象判别字段（判别字段 = *_schema 家族）
                other_kind_fields = {k for k in other.get("required", []) + list(other.get("properties", {}))
                                     if k.endswith("_schema")}
                self.assertEqual(set(), canon_kind_fields & other_kind_fields,
                                 "投影 %s 与 canonical %s 重复定义对象判别字段: %s"
                                 % (proj["file"], cls["canonical_schema_file"],
                                    sorted(canon_kind_fields & other_kind_fields)))
                # (iii) 投影不得把 canonical 的对象级 gate 整段搬走充当自己的定义
                canon_gates = set(canon.get("allOf", [{}])[0].get("anyOf", [{}])[0].keys()) if canon.get("allOf") else set()
                other_gates = set(other.get("allOf", [{}])[0].get("anyOf", [{}])[0].keys()) if other.get("allOf") else set()
                self.assertEqual(set(), canon_gates & other_gates)
                # (iv) 投影不得在任何共有属性上给出与 canonical 相同的判定值（对象级等价判定）
                for key in sorted((set(canon["properties"]) & set(other.get("properties", {}))) - {"schema_version"}):
                    cv = canon["properties"][key].get("const")
                    ov = other["properties"][key].get("const")
                    if cv is not None and cv == ov:
                        self.fail("投影 %s 与 canonical %s 在 %s 上判定相同 (=%r)：构成等价重复定义"
                                  % (proj["file"], cls["canonical_schema_file"], key, cv))


class TestAmbiguousFieldGuard(unittest.TestCase):
    """B. weight/value/mask/snr 单独出现即不合格（schema 内真实拦截）。"""

    def test_guard_is_present_in_every_object_schema(self):
        for o in OBJECTS:
            doc = load(UNIFIED / ("%s.schema.json" % o))
            pat = doc.get("propertyNames", {}).get("pattern")
            self.assertTrue(pat, "%s: 缺 propertyNames 歧义名守卫" % o)
            # pattern 是 propertyNames 守卫：不合格名的语义 = 守卫不匹配该名（validator 即判红）
            for bad in AMBIGUOUS:
                self.assertIsNone(re.search(pat, bad), "%s: 守卫 pattern 未拦住裸名 %r" % (o, bad))
                # 裸名后接 _xxx（未限定对象）同罪：weight_value / snr_median
                for unqualified in (bad + "_value", bad + "_median"):
                    self.assertIsNone(re.search(pat, unqualified),
                                      "%s: 守卫放行了未限定名 %r" % (o, unqualified))
                # 裸名带对象/类型限定前缀合格：frame_snr_value / variance_value / bad_pixel_mask
                for qualified in ("point_information_value", "variance_value",
                                  "bad_pixel_mask", "frame_snr_value"):
                    self.assertIsNotNone(re.search(pat, qualified),
                                         "%s: 守卫 pattern 误拦合格限定名 %r" % (o, qualified))

    def test_bare_ambiguous_key_is_rejected_by_every_object_schema(self):
        for o in OBJECTS:
            base = load(EXAMPLES / ("%s.example.json" % o)) if (EXAMPLES / ("%s.example.json" % o)).is_file() \
                else load(EXAMPLES / "signal.example.json")
            for bad in AMBIGUOUS:
                doc = dict(base)
                doc["unified_object"] = o
                doc["object_schema_id"] = "https://astrocs.local/schemas/unified/%s/v1" % o
                doc[bad] = 1.0
                errs = merged_errors(doc, load(UNIFIED / ("%s.schema.json" % o)))
                self.assertIn(bad, errs, "%s: 裸字段 %r 未被拦截 (%s)" % (o, bad, errs))
                # 必须由 propertyNames 守卫（pattern）判红，而不是只靠 additionalProperties 兜底
                self.assertIn("%s:pattern:" % bad, errs, "%s: 裸字段 %r 未触发 propertyNames 守卫 (%s)" % (o, bad, errs))

    def test_leaf_carriers_must_be_typed_envelopes(self):
        """任何名为 weight/value/mask/snr 的对象属性都必须是带 kind 判别的强类型封装，不得裸标量。"""
        offenders = []
        for p in sorted(UNIFIED.glob("*.schema.json")):
            doc = load(p)
            for key, sub in doc.get("properties", {}).items():
                if key in AMBIGUOUS:
                    offenders.append((p.name, key, sorted(sub.keys())))
        self.assertEqual([], offenders, "canonical 对象 schema 不得直接暴露裸名属性: %s" % offenders)


class TestNegativeFixtures(unittest.TestCase):
    """C. 四个负例各自必败，失败原因命中指定门。"""

    def expected(self):
        return load(NEGATIVE / "EXPECTED.json")

    def test_expected_index_covers_all_cases(self):
        exp = self.expected()
        self.assertEqual(6, len(exp),
                         "负例索引必须覆盖全部负例（含退役对象声明负例 n5、稀疏层相对语义负例 n6）")
        for name in exp:
            self.assertTrue((NEGATIVE / name).is_file(), "负例 fixture 缺失: %s" % name)

    def test_every_negative_fixture_is_rejected_with_expected_gate(self):
        reg = load(REPO / REGISTRY_REL)
        for name, spec in sorted(self.expected().items()):
            doc = load(NEGATIVE / name)
            schema = load(REPO / reg["port_contract_ref"]["canonical_schema_file"]) \
                if spec["schema"] == "port_contract" \
                else load(UNIFIED / ("%s.schema.json" % spec["schema"]))
            errs = merged_errors(doc, schema)
            self.assertTrue(errs, "负例 %s 竟然通过校验（门失效）" % name)
            for tok in spec["must_match"]:
                self.assertIn(tok, errs, "负例 %s 未命中预期门 %r: %s" % (name, tok, errs))

    def test_negative_02_03_are_the_wrong_object_entirely(self):
        """② snr 冒充 variance 与 ③ coverage 当 rejection 必须是"对象本体错"，不是细节错。"""
        for name, wrong_object, right_object in (
                ("n2_source_snr_as_variance.schema-violation.json", "source_snr", "variance"),
                ("n3_coverage_as_rejection.schema-violation.json", "coverage", "rejection")):
            doc = load(NEGATIVE / name)
            self.assertEqual(wrong_object, doc["unified_object"])
            errs = merged_errors(doc, load(UNIFIED / ("%s.schema.json" % right_object)))
            self.assertIn("unified_object", errs)
            self.assertIn(right_object, errs)

    def test_negative_04_port_intent_is_reciprocal(self):
        """④ 端口合同必须双向定义：variance 只连 variance；错连的端口 id 同时出现在"禁止"与"示例"两侧。"""
        reg = load(REPO / REGISTRY_REL)
        ports = {p["port_id"]: p for p in reg["port_contracts"]}
        self.assertIn("phase2.integrate.pixel_variance_in", ports)
        port = ports["phase2.integrate.pixel_variance_in"]
        self.assertEqual("variance", port["accepts_object"])
        self.assertEqual("source_snr", port["rejects_example"]["connected_object"])
        self.assertEqual(port["port_id"], port["rejects_example"]["port_id"])
        self.assertEqual(port["port_id"], port["accepts_example"]["port_id"])
        self.assertEqual("variance", port["accepts_example"]["connected_object"])
        neg = load(NEGATIVE / "n4_source_snr_into_variance_port.schema-violation.json")
        self.assertEqual(port["port_id"], neg["port_id"])
        self.assertEqual(port["accepts_object"], neg["accepts_object"])
        self.assertEqual(port["rejects_object"], neg["connected_object_document"]["unified_object"])



class TestSparseSnrAbsoluteSemantics(unittest.TestCase):
    """G. sparse_snr_layer 存**绝对** SNR：schema 冻结 + 数值判据能红能绿。

    设计（ASTROCS_DESIGN.md §2.2/§3.1/§4.4/§5.3）：控制点值 = 该点的绝对通量型
    SNR F_ref/σ_F(x,y)，与 frame_snr 同口径、同逐帧参考通量 F_ref；Phase2 由
    控制点**直接重建**为稠密 SNR 场，**不**乘/除帧级标量。本类给出机器可判据：
      * 结构判据：schema 以 sparse_snr_semantics=absolute_flux_type_snr 冻结语义；
      * 数值判据：节点处重建值必须等于落盘值，且与帧级标量**无关**；
        按相对值解释（frame_snr × v/median(v)）时节点重建值 ≠ 落盘值 ⇒ 判红；
        并显式处理退化点（frame_snr == median(v) 时两解释重合，该处不计证据）。
    """

    def layer(self):
        return load(EXAMPLES / "sparse_snr_layer.example.json")

    def schema(self):
        return load(UNIFIED / "sparse_snr_layer.schema.json")

    # ── 结构判据 ──────────────────────────────────────────────────────────
    def test_schema_freezes_absolute_value_semantics(self):
        doc = self.schema()
        self.assertIn("sparse_snr_semantics", doc["required"],
                      "sparse_snr_semantics 必须 required（否则相对值解释可静默通过）")
        self.assertEqual("absolute_flux_type_snr",
                         doc["properties"]["sparse_snr_semantics"]["const"])
        cp = doc["properties"]["control_points"]["items"]["properties"]["sparse_snr_value"]
        self.assertIn("exclusiveMinimum", cp, "控制点值是绝对 SNR ⇒ 值域为 (0, +inf)")
        self.assertEqual(0, cp["exclusiveMinimum"])
        # 正例必须带该判别式
        self.assertEqual("absolute_flux_type_snr", self.layer()["sparse_snr_semantics"])
        self.assertEqual("", merged_errors(self.layer(), doc))

    def test_relative_semantics_declaration_is_rejected(self):
        """注入「仍按相对值解释」⇒ 必须判红（且命中语义判别门）。"""
        doc = self.schema()
        bad = json.loads(json.dumps(self.layer()))
        bad["sparse_snr_semantics"] = "relative_to_frame_snr_median"
        errs = merged_errors(bad, doc)
        self.assertIn("sparse_snr_semantics:const", errs,
                      "相对语义声明未命中 const 门: %s" % errs)

    def test_missing_semantics_declaration_is_rejected(self):
        """缺判别式 ⇒ 必须判红（禁「不说就是绝对」的静默通过）。"""
        doc = self.schema()
        bad = json.loads(json.dumps(self.layer()))
        del bad["sparse_snr_semantics"]
        errs = merged_errors(bad, doc)
        self.assertIn("sparse_snr_semantics", errs, "缺判别式未判红: %s" % errs)

    def test_zero_control_point_value_is_rejected(self):
        """0 不是合法绝对 SNR（invalid 由 NaN 承载，禁 0 冒充）。"""
        doc = self.schema()
        bad = json.loads(json.dumps(self.layer()))
        bad["control_points"][0]["sparse_snr_value"] = 0.0
        errs = merged_errors(bad, doc)
        self.assertIn("sparse_snr_value", errs, "0 值未判红: %s" % errs)

    # ── 数值判据（能红能绿，含退化点处置）────────────────────────────────
    @staticmethod
    def node_reconstruction(points, frame_snr, interpretation):
        """控制点处的重建值。absolute = 落盘绝对值的直接重建；relative = 被否决的乘帧级解释。"""
        vals = [p["sparse_snr_value"] for p in points]
        med = sorted(vals)[len(vals) // 2]
        if interpretation == "absolute":
            return list(vals)
        if interpretation == "relative":
            return [frame_snr * v / med for v in vals]
        raise ValueError(interpretation)

    @staticmethod
    def verdict(points, frame_snr):
        """判据：绝对解释下节点重建 == 落盘值，且与 frame_snr 无关 ⇒ GREEN；否则 RED。

        返回 (verdict, evidence)；frame_snr == median(v) 时两解释重合 ⇒ DEGENERATE（不计证据）。
        """
        vals = [p["sparse_snr_value"] for p in points]
        med = sorted(vals)[len(vals) // 2]
        if abs(frame_snr - med) <= 0.0:
            return "DEGENERATE", {"median": med, "frame_snr": frame_snr}
        abs_v = TestSparseSnrAbsoluteSemantics.node_reconstruction(points, frame_snr, "absolute")
        rel_v = TestSparseSnrAbsoluteSemantics.node_reconstruction(points, frame_snr, "relative")
        node_ok = all(a == b for a, b in zip(abs_v, vals))
        frame_invariant = (abs_v ==
                           TestSparseSnrAbsoluteSemantics.node_reconstruction(points, frame_snr * 7.0, "absolute"))
        differs = any(abs(a - r) > 0.0 for a, r in zip(abs_v, rel_v))
        ok = node_ok and frame_invariant and differs
        return ("GREEN" if ok else "RED"), \
            {"node_reproduction": node_ok, "frame_invariant": frame_invariant,
             "relative_interpretation_differs": differs,
             "max_abs_delta_relative_vs_absolute": max(abs(a - r) for a, r in zip(abs_v, rel_v))}

    def test_absolute_reconstruction_is_green_and_relative_is_red(self):
        pts = self.layer()["control_points"]
        v, ev = self.verdict(pts, 100.0)          # frame_snr 故意 != median(22.5, 19.75)=21.125
        self.assertEqual("GREEN", v, "绝对解释未通过节点复现/帧级无关判据: %s" % ev)
        self.assertTrue(ev["relative_interpretation_differs"])
        self.assertGreater(ev["max_abs_delta_relative_vs_absolute"], 0.0,
                           "相对解释与绝对解释不可区分 ⇒ 判据退化")

    def test_frame_scalar_does_not_change_reconstruction(self):
        """同一稀疏层配不同帧级标量 ⇒ 重建结果必须逐位相同（帧级标量不是尺度基准）。"""
        pts = self.layer()["control_points"]
        a = self.node_reconstruction(pts, 5.0, "absolute")
        b = self.node_reconstruction(pts, 5000.0, "absolute")
        self.assertEqual(a, b)

    def test_degenerate_point_is_not_evidence(self):
        """退化点（frame_snr == median(v)）两解释重合 ⇒ 判据必须报 DEGENERATE，不得当绿。"""
        pts = self.layer()["control_points"]
        med = sorted(p["sparse_snr_value"] for p in pts)[len(pts) // 2]
        v, _ = self.verdict(pts, med)
        self.assertEqual("DEGENERATE", v, "真值无效应时必须退化，不得给出恒真绿")

    def test_relative_interpretation_red_is_live(self):
        """负例活体检查：把相对解释当正解 ⇒ 与落盘绝对值不同（判据可判红）。"""
        pts = self.layer()["control_points"]
        vals = [p["sparse_snr_value"] for p in pts]
        rel = self.node_reconstruction(pts, 100.0, "relative")
        self.assertNotEqual(rel, vals,
                            "相对解释与落盘绝对值相同 ⇒ 无法判红（判据失效）")

    # ── 文档漂移判据（注入相对表述 ⇒ 判红）──────────────────────────────
    AUTHORITY_DOCS = (
        "ASTROCS_DESIGN.md",
        "docs/design/UNIFIED_MODEL.md",
        "docs/design/PHASE2_DETAILED_DESIGN.md",
        "docs/plugins/algorithms_phase1/07_noise_snr.md",
        "docs/plugins/algorithms_phase2/13_integration.md",
        "docs/contracts/DATA_SEMANTICS.md",
        "docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md",
    )
    RELATIVE_DECLARATIONS = ("帧内相对场", "稀疏相对 SNR", "相对 SNR 层", "中位归一",
                             "rho_c", "SNR_frame × rho", "帧级 × 帧内")

    def test_authority_docs_declare_absolute_not_relative(self):
        """权威链文档不得把稀疏层声明为相对场/中位归一（注入该表述 ⇒ 必须判红）。"""
        offenders = []
        for rel in self.AUTHORITY_DOCS:
            p = REPO / rel
            self.assertTrue(p.is_file(), "权威文档缺失（fail-closed）: %s" % rel)
            txt = p.read_text(encoding="utf-8")
            for tok in self.RELATIVE_DECLARATIONS:
                if tok in txt:
                    offenders.append("%s: %r" % (rel, tok))
        self.assertEqual([], offenders, "权威链文档仍把稀疏层声明为相对场: %s" % offenders)

    def test_doc_drift_judge_is_live(self):
        """判据活体：注入一处相对表述 ⇒ 同一判据必须命中（能红）。"""
        fake = "docs/plugins/algorithms_phase1/07_noise_snr.md"
        txt = (REPO / fake).read_text(encoding="utf-8")
        injected = txt + "\n- 稀疏层与帧级标量的关系是「帧级 × 帧内相对场」：帧内相对场中位归一。\n"
        hits = [t for t in self.RELATIVE_DECLARATIONS if t in injected]
        self.assertIn("帧内相对场", hits, "注入相对表述后判据未命中 ⇒ 判据失效")
        self.assertIn("中位归一", hits)

class TestRetiredObjectContract(unittest.TestCase):
    """F. 退役对象（psfsw_robust_weight，14→13）：无 canonical 正本、旧声明显式拒绝 + 迁移提示。"""

    RETIRED = "psfsw_robust_weight"
    RETIRED_SCHEMA = "eng/contracts/schemas/unified/psfsw_robust_weight.schema.json"
    RETIRED_EXAMPLE = "eng/contracts/schemas/unified/examples/psfsw_robust_weight.example.json"

    def retired_entry(self):
        m = load(REPO / "eng/contracts/data/unified_object_compatibility_map_v1.json")
        self.assertEqual(1, len(m["retired_entries"]), "退役记录必须逐条登记（不得静默删除）")
        e = m["retired_entries"][0]
        self.assertEqual(self.RETIRED, e["covers_object"])
        return e

    def test_retired_object_has_no_canonical_face(self):
        """fail-closed：canonical schema / example 不存在、schema ID 不再出现、不在 canonical 清单。"""
        self.assertNotIn(self.RETIRED, OBJECTS)
        self.assertFalse((REPO / self.RETIRED_SCHEMA).exists(), "退役对象的 canonical schema 不得复活")
        self.assertFalse((REPO / self.RETIRED_EXAMPLE).exists(), "退役对象的正例不得复活")
        sid = "https://astrocs.local/schemas/unified/%s/v1" % self.RETIRED
        for p in sorted(SCHEMAS.rglob("*.schema.json")):
            self.assertNotEqual(sid, load(p).get("$id"), "退役对象的 schema ID 仍被声明: %s" % p)
        reg = load(REPO / REGISTRY_REL)
        self.assertNotIn(self.RETIRED, {c["object_name"] for c in reg["canonical_object_classes"]})

    def test_retired_object_declaration_is_explicitly_rejected(self):
        """旧产品/端口声明退役对象 ⇒ 端口合同 enum 判红（显式拒绝，不是"恰好别处报错"）。"""
        port = load(UNIFIED / "port_contract.schema.json")
        neg = load(NEGATIVE / "n5_retired_psfsw_robust_weight.schema-violation.json")
        self.assertEqual(self.RETIRED, neg["connected_object_document"]["unified_object"])
        errs = merged_errors(neg, port)
        self.assertTrue(errs)
        for tok in ("accepts_object:enum", "accepts_schema_id:enum",
                    "connected_object_document/unified_object:enum",
                    "connected_object_document/object_schema_id:enum"):
            self.assertIn(tok, errs, "退役对象声明未命中预期拒绝门 %r: %s" % (tok, errs))
        # 反向：同形端口连接合法对象必须通过（证明判红来自对象枚举，不是端口形态本身）
        ok = json.loads(json.dumps(neg))
        ok["accepts_object"] = "point_information"
        ok["accepts_schema_id"] = "https://astrocs.local/schemas/unified/point_information/v1"
        ok["connected_object_document"]["unified_object"] = "point_information"
        ok["connected_object_document"]["object_schema_id"] = \
            "https://astrocs.local/schemas/unified/point_information/v1"
        self.assertEqual("", merged_errors(ok, port))

    def test_retirement_record_carries_migration_hint(self):
        """退役必须带变更编号 + 拒绝策略 + 迁移提示（不得静默接受）。"""
        e = self.retired_entry()
        self.assertEqual("retired_canonical_object", e["relation"])
        self.assertTrue(e["retired_at_change"].startswith("CHG-"), "退役必须绑定变更编号")
        self.assertIn("显式拒绝", e["rejection"])
        self.assertIn(self.RETIRED, e["rejection"])
        self.assertIn("迁移提示", e["migration_hint"])
        self.assertIn("point_information", e["migration_hint"])
        self.assertFalse((REPO / e["canonical_schema"]).exists(),
                         "退役记录的 canonical schema 必须确实不存在（fail-closed）")
        reg = load(REPO / REGISTRY_REL)
        rec = reg["deprecation"]["object_retirements"][self.RETIRED]
        self.assertEqual(e["retired_at_change"], rec["retired_at_change"])
        self.assertEqual(e["migration_hint"], rec["migration_hint"])
        self.assertEqual("retired", rec["state"], "退役状态必须是机器可读 ASCII token")

    def test_retired_name_not_reintroduced_in_canonical_face(self):
        """退役对象名不得以 canonical 形式回流（canonical schema / 正例；留痕必须含「已退役」或退役容器）。"""
        offenders = []
        containers = ("retired_objects", "retired_entries", "object_retirements")

        def walk(node, path):
            if isinstance(node, dict):
                for k, v in node.items():
                    if self.RETIRED in k and not (set(path) & set(containers)):
                        offenders.append("%s#%s" % ("/".join(str(x) for x in path), k))
                    walk(v, path + (k,))
            elif isinstance(node, list):
                for i, v in enumerate(node):
                    walk(v, path + (i,))
            elif isinstance(node, str) and self.RETIRED in node:
                if not (set(path) & set(containers)) \
                        and "已退役" not in node and "retired" not in node:
                    offenders.append("%s=%r" % ("/".join(str(x) for x in path), node[:60]))

        for p in sorted(list(UNIFIED.glob("*.schema.json")) + list(EXAMPLES.glob("*.example.json"))):
            walk(load(p), (p.relative_to(REPO).as_posix(),))
        self.assertEqual([], offenders, "canonical 面出现未留痕的退役对象名: %s" % offenders)


class TestUnitsMissingPrecisionWeight(unittest.TestCase):
    """D. 单位（含 BUNIT 语义）/ 无效值 / 精度 / 可否作权重 与 UNIFIED_MODEL §2 一致。"""

    def test_weight_column_matches_unified_model_verdicts(self):
        for o in OBJECTS:
            doc = load(UNIFIED / ("%s.schema.json" % o))
            verdict, eligible = VERDICT[o]
            self.assertEqual(verdict, doc["properties"]["object_weight_verdict"]["const"],
                             "%s: 可否作权重判定被改动（必须照抄 UNIFIED_MODEL §2）" % o)
            self.assertEqual(eligible, doc["properties"]["object_weight_capability"]["const"])
            self.assertEqual(verdict, doc["x-astrocs-object"]["weight_verdict"])
            self.assertIn("object_weight_capability", doc["required"])
            self.assertIn("object_weight_verdict", doc["required"])

    def test_units_block_carries_bunit_semantics(self):
        for o in OBJECTS:
            doc = load(UNIFIED / ("%s.schema.json" % o))
            units = doc["properties"]["units"]
            self.assertIn("units", doc["required"], "%s: units 必须 required" % o)
            for k in ("bunit", "bunit_semantics", "pixel_semantics", "pixel_area_power"):
                self.assertIn(k, units["required"], "%s: units.%s 必须 required" % (o, k))
            self.assertEqual(["written_px_power", "declared_via_provenance"],
                             units["properties"]["bunit_semantics"]["enum"])
            self.assertIn("allOf", units, "%s: BUNIT 语义必须有机器门（if/then）" % o)

    def test_missing_value_block_declared(self):
        for o in OBJECTS:
            doc = load(UNIFIED / ("%s.schema.json" % o))
            mv = doc["properties"]["missing_value"]
            self.assertIn("missing_value", doc["required"])
            self.assertIn("missing_repr", mv["required"])
            self.assertIn("invalid_repr", mv["required"])

    def test_precision_declared(self):
        for o in OBJECTS:
            doc = load(UNIFIED / ("%s.schema.json" % o))
            self.assertEqual(["float32", "float64", "integer"], doc["properties"]["precision"]["enum"])
            self.assertIn("precision", doc["required"])

    def test_all_positive_examples_pass(self):
        got = sorted(p.name for p in EXAMPLES.glob("*.example.json"))
        self.assertGreaterEqual(len(got), len(OBJECTS) + 1,
                                "正例不足 %d 份（13 个对象 + signal_flux 变体）: %s" % (len(OBJECTS) + 1, got))
        for p in sorted(EXAMPLES.glob("*.example.json")):
            doc = load(p)
            schema = load(UNIFIED / ("%s.schema.json" % doc["unified_object"]))
            errs = merged_errors(doc, schema)
            self.assertEqual("", errs, "正例 %s 未通过: %s" % (p.name, errs))

    def test_examples_cover_all_canonical_objects(self):
        covered = {load(p)["unified_object"] for p in EXAMPLES.glob("*.example.json")}
        self.assertEqual(sorted(OBJECTS), sorted(covered))

    def test_bunit_dimension_gate_is_live(self):
        """BUNIT 量纲不可判（ADU 且无 pixel 语义声明）必须判红。"""
        doc = load(EXAMPLES / "signal.example.json")
        doc["units"] = {"bunit": "ADU", "bunit_semantics": "declared_via_provenance",
                        "pixel_semantics": "surface_brightness", "pixel_area_power": -2}
        errs = merged_errors(doc, load(UNIFIED / "signal.schema.json"))
        self.assertIn("target_pixel_area", errs, "缺目标像素面积（BUNIT=ADU 面亮度）应判红: %s" % errs)


class TestDeclarationDriftGuard(unittest.TestCase):
    """登记表（registry / 语义文档 / DATA_ARTIFACTS）不得与 13 个 canonical schema 漂移。"""

    def setUp(self):
        self.reg = load(REPO / REGISTRY_REL)
        self.doc = (REPO / "docs/contracts/UNIFIED_OBJECTS.md").read_text(encoding="utf-8")
        self.da = (REPO / "docs/contracts/DATA_ARTIFACTS.md").read_text(encoding="utf-8")

    def test_object_id_pattern_is_derived_from_object_name(self):
        for c in self.reg["canonical_object_classes"]:
            self.assertEqual("DATA-OBJ-%s-001" % c["object_name"].upper().replace("_", "-"),
                             c["object_data_id"])
            self.assertEqual("https://astrocs.local/schemas/unified/%s/v1" % c["object_name"], c["schema_id"])

    def test_registry_field_claims_match_schema(self):
        for c in self.reg["canonical_object_classes"]:
            doc = load(REPO / c["canonical_schema_file"])
            units = doc["properties"]["units"]["properties"]
            if c["unit_bunit"].startswith("ADU^-2"):
                self.assertEqual("ADU^-2", units["bunit"]["const"])
            self.assertEqual(c["pixel_semantics"], units["pixel_semantics"]["const"]
                             if "const" in units["pixel_semantics"] else c["pixel_semantics"])
            self.assertEqual(c["weight_verdict"], doc["properties"]["object_weight_verdict"]["const"])
            self.assertEqual(c["weight_capable"], doc["properties"]["object_weight_capability"]["const"])
            for pr in c["precision"].split("|"):
                self.assertIn(pr, doc["properties"]["precision"]["enum"])

    def test_doc_and_data_artifacts_agree_with_registry(self):
        for c in self.reg["canonical_object_classes"]:
            self.assertIn(c["object_name"], self.doc)
            self.assertIn(c["schema_id"], self.doc)
            self.assertIn(c["weight_verdict"], self.doc)
            self.assertIn("| %s |" % c["object_data_id"], self.da)


class TestConfigSeparationAnchors(unittest.TestCase):
    """E. 三类配置不得共用字段名承载不同含义（schema 本体归 CFG-001）。"""

    def setUp(self):
        self.reg = load(REPO / "docs/contracts/config_separation_anchors.json")

    def test_three_classes_present_and_owned_by_cfg001(self):
        classes = self.reg["config_classes"]
        self.assertEqual(["phase_config", "cpu_profile", "run_manifest"],
                         [c["class"] for c in classes])
        for c in classes:
            self.assertEqual("CFG-001", c["schema_owner"])

    def test_field_name_sets_are_pairwise_disjoint(self):
        """字段名模式互斥，且每个模式的 examples 必须自洽（本类匹配、他类不匹配）。"""
        classes = self.reg["config_classes"]
        for c in classes:
            others = [o for o in classes if o["class"] != c["class"]]
            for spec in c["field_name_patterns"]:
                for ex in spec["examples"]:
                    self.assertRegex(ex, spec["regex"], "%s: 例子 %r 不匹配自己的模式" % (c["class"], ex))
                    for o in others:
                        for ospec in o["field_name_patterns"]:
                            self.assertIsNone(re.compile(ospec["regex"]).search(ex),
                                              "%s 与 %s 字段名冲突: %r 同时匹配 %s"
                                              % (c["class"], o["class"], ex, ospec["regex"]))
        for c in classes:
            for bad in c["forbidden_field_names"]:
                self.assertFalse(any(re.compile(s["regex"]).search(bad) for s in c["field_name_patterns"]),
                                 "%s: 禁止名 %r 被自己的模式放行" % (c["class"], bad))

    def test_hardware_names_forbidden_in_phase_config(self):
        banned = self.reg["cross_class_forbidden"]["cpu_profile_keys_in_phase_config"]
        for bad in ("isa", "workers", "block", "cpu_model", "thread_budget"):
            self.assertIn(bad, banned)
        phase = next(c for c in self.reg["config_classes"] if c["class"] == "phase_config")
        for bad in banned:
            self.assertIn(bad, phase["forbidden_field_names"], "%s 未登记为 phase_config 禁止名" % bad)
            for spec in phase["field_name_patterns"]:
                self.assertIsNone(re.compile(spec["regex"]).search(bad),
                                  "phase_config 模式 %s 放行了硬件字段名 %r" % (spec["regex"], bad))

    def test_run_manifest_freezes_hashes_only(self):
        c = next(c for c in self.reg["config_classes"] if c["class"] == "run_manifest")
        joined = " ".join(ex for spec in c["field_name_patterns"] for ex in spec["examples"])
        for token in ("software_sha", "config_hash", "manifest_input_hashes", "manifest_output_hashes"):
            self.assertIn(token, joined)


class TestRegistryIndex(unittest.TestCase):
    """registry 索引与 INDEX.yaml / DATA_ARTIFACTS.md 的一致性（迁移映射与废弃/退役登记）。

    废弃登记 = 变更编号（CHG-YYYY-MM-DD-<TAG>）；退役条件 = 负责人裁决哨兵 OWNER_DECISION
    —— 版本号不得作为生效/退役条件（ASTROCS_DESIGN.md §12；GAP_AUDIT §4.1 Q2 裁决）。
    """

    def test_registry_has_deprecation_window(self):
        reg = load(REPO / REGISTRY_REL)
        dep = reg["deprecation"]
        self.assertTrue(dep["legacy_paths_are_compatibility_only"])
        self.assertTrue(dep["no_second_equivalent_schema"])
        # 本对象只有 retire_after（无 deprecated_at）⇒ 只锁退役哨兵与说明句。
        self.assertEqual("OWNER_DECISION", dep["retire_after_change"])
        self.assertIn("版本号不得作为退役条件", dep["retire_note"])
        self.assertGreaterEqual(len(dep["legacy_paths"]), 10)
        # 12 条旧路径逐条锁：废弃 = 变更编号；退役 = 负责人裁决哨兵（不得写版本号窗口）。
        self.assertEqual(12, len(dep["legacy_paths"]))
        for lp in dep["legacy_paths"]:
            self.assertRegex(lp["deprecated_at_change"], r"^CHG-\d{4}-\d{2}-\d{2}-[A-Z0-9-]+$")
            self.assertEqual("OWNER_DECISION", lp["retire_after_change"])
            self.assertIn("版本号不得作为退役条件", lp["retire_note"])
        # 产品族专用投影面必须与磁盘逐一对应（非对象 schema 一律走 other_schema_files 登记），
        # 且每条投影不得写版本号退役窗口。判据比"固定条数"更强：条数随出库变化，覆盖关系不随之松动。
        projections = [p for c in reg["canonical_object_classes"]
                       for p in c["compatibility_projections"]]
        canonical_files = {c["canonical_schema_file"] for c in reg["canonical_object_classes"]}
        owned_other = {o["file"] for o in reg["other_schema_files"]}
        on_disk = {p.relative_to(REPO).as_posix() for p in SCHEMAS.rglob("*.schema.json")}
        expected = on_disk - canonical_files - owned_other
        self.assertEqual(expected, {p["file"] for p in projections},
                         "compatibility_projections 必须与磁盘上的兼容期 schema 面逐一对应: "
                         "expected=%s declared=%s" % (sorted(expected), sorted(p["file"] for p in projections)))
        for p in projections:
            self.assertEqual("FROZEN_COMPATIBILITY", p["status"])
            self.assertEqual("OWNER_DECISION", p["retire_after_change"])
            self.assertIn("版本号不得作为退役条件", p["retire_note"])

    def test_no_version_window_literals_in_registration_files(self):
        """反向锁（Q2 裁决：删版本号退役窗口）：两个登记件内不得再出现任何 0.x.y 版本号。

        能红能绿：把任一 deprecated_at_change / retire_after_change 改回版本号字面量
        ⇒ 本用例必红；还原 ⇒ 判绿。
        """
        reg = load(REPO / REGISTRY_REL)
        m = load(REPO / "eng/contracts/data/unified_object_compatibility_map_v1.json")
        self.assertNotRegex(json.dumps(reg, ensure_ascii=False), r"\b0\.\d+\.\d+\b",
                            "registry 仍含版本号退役窗口字面量（Q2：改用变更编号/日期）")
        self.assertNotRegex(json.dumps(m, ensure_ascii=False), r"\b0\.\d+\.\d+\b",
                            "兼容期映射仍含版本号退役窗口字面量（Q2：改用变更编号/日期）")

    def test_registry_object_ids_are_unique(self):
        reg = load(REPO / REGISTRY_REL)
        ids = [c["object_data_id"] for c in reg["canonical_object_classes"]]
        self.assertEqual(len(ids), len(set(ids)))
        for i in ids:
            self.assertRegex(i, r"^DATA-OBJ-[A-Z0-9-]+-\d{3}$")

    def test_index_yaml_lists_object_ids_and_new_paths(self):
        text = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")
        reg = load(REPO / REGISTRY_REL)
        for c in reg["canonical_object_classes"]:
            self.assertIn(c["object_data_id"], text, "INDEX.yaml 缺对象登记: %s" % c["object_data_id"])
            self.assertIn(c["canonical_schema_file"], text, "INDEX.yaml 缺新路径: %s" % c["canonical_schema_file"])

    def test_contracts_data_is_compatibility_only(self):
        """eng/contracts/data/** 只能是兼容期映射：每条登记必须指向 canonical schema，且不得自称为对象定义。"""
        m = load(REPO / "eng/contracts/data/unified_object_compatibility_map_v1.json")
        reg = load(REPO / REGISTRY_REL)
        by_name = {c["object_name"]: c for c in reg["canonical_object_classes"]}
        # 基线实测 10 个 eng/contracts/data 条目 = 现存 9 条 + 退役 1 条（psfsw_robust_weight，14→13）
        self.assertEqual(9, len(m["entries"]), "兼容期映射现存条目数漂移（基线 10 = 9 现存 + 1 退役）")
        self.assertEqual(10, len(m["entries"]) + len(m["retired_entries"]),
                         "基线 10 个 eng/contracts/data 条目必须逐条有归宿（现存 + 退役）")
        for e in m["entries"]:
            self.assertIn(e["covers_object"], OBJECTS)
            cls = by_name[e["covers_object"]]
            self.assertEqual(cls["canonical_schema_file"], e["canonical_schema"])
            self.assertEqual(cls["schema_id"], e["canonical_schema_id"])
            self.assertEqual("compatibility_view_of_canonical_object", e["relation"])
            self.assertRegex(e["deprecated_at_change"], r"^CHG-\d{4}-\d{2}-\d{2}-[A-Z0-9-]+$")
            self.assertEqual("OWNER_DECISION", e["retire_after_change"])
            self.assertIn("版本号不得作为退役条件", e["retire_note"])
            self.assertTrue(e["evidence"])
        # 7 条 legacy_contract_id_map 旧 ID 同批口径：废弃 = 变更编号；退役 = 负责人裁决哨兵。
        for row in m["legacy_contract_id_map"]:
            self.assertRegex(row["deprecated_at_change"], r"^CHG-\d{4}-\d{2}-\d{2}-[A-Z0-9-]+$")
            self.assertEqual("OWNER_DECISION", row["retire_after_change"])
            self.assertIn("版本号不得作为退役条件", row["retire_note"])

    def test_legacy_ids_are_decided_not_pending(self):
        """追加门（负责人裁决）：7 个旧 ID 必须逐个给出 mapped 或 no_canonical，不得含糊。"""
        reg = load(REPO / REGISTRY_REL)
        sec = reg["legacy_contract_ids"]
        want = ["DATA-GAIA-001", "DATA-P1-CAL", "DATA-P1-COS", "DATA-P1-COSMETIC",
                "DATA-P1-DRZ", "DATA-P1-SOURCES", "DATA-P2-SMP"]
        entries = {e["legacy_id"]: e for e in sec["entries"]}
        self.assertEqual(sorted(want), sorted(entries))
        self.assertEqual({"total": 7, "mapped": 4, "no_canonical": 3,
                          "mapped_ids": ["DATA-P1-CAL", "DATA-P1-COS", "DATA-P1-COSMETIC", "DATA-P1-DRZ"],
                          "no_canonical_ids": ["DATA-P1-SOURCES", "DATA-P2-SMP", "DATA-GAIA-001"]},
                         sec["summary"])
        for lid, e in entries.items():
            self.assertIn(e["decision"], ("mapped", "no_canonical"))
            self.assertNotIn("待定", e["note"])
            self.assertTrue(e["old_contract_face"], "%s: 缺旧合同面证据" % lid)
            self.assertTrue(e["evidence"], "%s: 缺判定证据" % lid)
            if e["decision"] == "mapped":
                self.assertTrue(e["canonical_objects"], "%s: mapped 必须给出对象" % lid)
                for o in e["canonical_objects"]:
                    self.assertIn(o, OBJECTS)
                    self.assertTrue((UNIFIED / ("%s.schema.json" % o)).is_file())
            else:
                self.assertEqual([], e["canonical_objects"])
                self.assertEqual("no_canonical", e["decision"])

    def test_legacy_evidence_is_not_self_referential(self):
        """证据更正门：old_contract_face 只能是基线旧面命中；本任务新增的索引行必须另立字段。"""
        reg = load(REPO / REGISTRY_REL)
        m = load(REPO / "eng/contracts/data/unified_object_compatibility_map_v1.json")
        rows = {r["legacy_id"]: r for r in m["legacy_contract_id_map"]}
        allowed = ("docs/contracts/DATA_SEMANTICS.md", "docs/contracts/PUBLIC_API.md",
                   "docs/contracts/DATA_ARTIFACTS.md", "docs/modules/MODULE_MAP.yaml",
                   "docs/modules/registry/", "docs/modules/", "docs/traceability/TRACEABILITY_MATRIX")
        for e in reg["legacy_contract_ids"]["entries"]:
            lid = e["legacy_id"]
            self.assertTrue(e["new_index_added_by_DATA001"], "%s: 缺 new_index_added_by_DATA001 字段" % lid)
            for src in list(e["old_contract_face"]) + list(rows[lid]["old_contract_face"]):
                self.assertFalse(src.startswith("docs/contracts/INDEX.yaml"),
                                 "%s: old_contract_face 引用本任务新增的 INDEX.yaml 属循环自证: %r" % (lid, src))
                self.assertTrue(any(src.startswith(a) for a in allowed),
                                "%s: old_contract_face 非基线旧面证据: %r" % (lid, src))
            for src in e["new_index_added_by_DATA001"]:
                self.assertIn("INDEX.yaml", src)
            self.assertEqual(e["new_index_added_by_DATA001"], rows[lid]["new_index_added_by_DATA001"])

    def test_legacy_id_map_written_into_index_and_compat_map(self):
        """映射必须同时落在 docs/contracts/INDEX.yaml 与 eng/contracts/data/unified_object_compatibility_map_v1.json。"""
        reg = load(REPO / REGISTRY_REL)
        idx = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")
        m = load(REPO / "eng/contracts/data/unified_object_compatibility_map_v1.json")
        rows = {r["legacy_id"]: r for r in m["legacy_contract_id_map"]}
        for e in reg["legacy_contract_ids"]["entries"]:
            lid = e["legacy_id"]
            self.assertIn(lid, idx, "INDEX.yaml 缺旧 ID 映射: %s" % lid)
            self.assertIn("DATA001-LEGACY-ID: %s" % lid, idx)
            self.assertIn(lid, rows, "兼容期映射缺旧 ID: %s" % lid)
            row = rows[lid]
            self.assertEqual(e["decision"], row["decision"])
            self.assertEqual(e["canonical_objects"], row["canonical_objects"])
            for f_ in row["canonical_schema_files"]:
                self.assertTrue((REPO / f_).is_file(), "映射指向不存在的 schema: %s" % f_)
        self.assertEqual(reg["legacy_contract_ids"]["summary"], m["legacy_contract_id_summary"])
        self.assertEqual({c["object_name"]: c["schema_id"] for c in reg["canonical_object_classes"]},
                         m["canonical_object_schema_ids"])

    def test_data_artifacts_registers_object_ids(self):
        text = (REPO / "docs/contracts/DATA_ARTIFACTS.md").read_text(encoding="utf-8")
        reg = load(REPO / REGISTRY_REL)
        cols = ["schema_id", "内容", "scalar", "shape/axis", "unit", "coordinate", "invalid",
                "ownership", "serialization"]
        header = "| " + " | ".join(cols) + " |"
        self.assertIn(header, text, "DATA_ARTIFACTS.md 对象登记表头缺失")
        for c in reg["canonical_object_classes"]:
            self.assertIn("| %s |" % c["object_data_id"], text)


if __name__ == "__main__":
    unittest.main(verbosity=2)
