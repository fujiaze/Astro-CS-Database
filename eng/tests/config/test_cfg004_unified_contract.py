#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CFG-004 三命令同构输入合同（GAP_AUDIT §9.71 裁决 2 / ASTROCS_DESIGN.md §3.3）机器门。

裁决语义（§9.71 裁决 2 逐字）：「HiPS类似阶段一，因为有不同滤镜，而输出产物是单帧HiPS。
所以**可以依然用块状结构**，**输出名称，运行参数+输入的一组帧构成一个大括号块**。」
定案 3：键名须与 CLI 统一；定案 4：**不是「退役旧合同」**，而是把三套口径统一到「块状结构」。

本模块锁 schema/模板面（CLI 运行面见 eng/tests/cli/test_unified_blocks_templates.py）：
  ① 三份 schema 同构三分支：blocks[] / 平铺单块简写 / 旧合同分支（mosaic·export 保留；normalize 由 §9.68 退役）
  ② 块内键集 = 该会话 config_fields()（唯一声明 session_commands.h）+ parser.cpp block_keys()
     —— **禁止第二份键名清单**：本测试从 CLI 源码现场派生并逐字比对
  ③ 按 §9.73 裁决 A44 排除 weight_mode / legacy_allow_weight_fallback / algorithm_weight_mode
  ④ 排异 min/max 禁用（§9.71 裁决 3 + DESIGN-DRAFT §1.8）⇒ 旧 enum 的 minmax 已删、新分支不收
  ⑤ 新分支不出现 phase_name / config / inputs[].product / inputs[].filter
  ⑥ 模板 = 块形态且键集与同一份键表同源（只允许省略已登记死键）
  ⑦ 旧合同分支保留且仍通过 schema（登记点/默认值指针继续可解析）
  ⑧ 平铺简写与 blocks[] 的 CLI 两门必须同源于一份按会话键表（差异只许以 FLAT_ONLY_RESIDUE 登记）

跑法：python3 -m unittest discover -s eng/tests/config -t eng/tests/config
"""
import json
import re
import unittest

import cfg_common as C

PHASE_SCHEMAS = {
    "normalize": "eng/contracts/schemas/phase_config_normalize.schema.json",
    "mosaic": "eng/contracts/schemas/phase_config_mosaic.schema.json",
    "export": "eng/contracts/schemas/phase_config_export.schema.json",
}
TEMPLATES = {
    "normalize": "eng/packaging/config/templates/normalize.phase_config.json",
    "mosaic": "eng/packaging/config/templates/mosaic.phase_config.json",
    "export": "eng/packaging/config/templates/export.phase_config.json",
}
INPUT_KEY = {"normalize": "input_lights", "mosaic": "hips_paths", "export": "source"}
POS = "eng/tests/config/fixtures/positive/"
NEG = "eng/tests/config/fixtures/negative/"
SESSION_H = "lib/infrastructure/cli/session_commands.h"
PARSER_CPP = "lib/infrastructure/cli/parser.cpp"
REGISTRY = "eng/packaging/config/config_registry.json"
DEFAULTS = "eng/packaging/config/defaults.json"
LEDGER = "eng/ci/ledgers/dead_config_keys.json"
PROJ_REGISTRY = "eng/contracts/schemas/projection_registry.schema.json"

# §9.73 裁决 A44（负责人 2026-09-20）：「权重模式」概念不存在 ⇒ 三套键名一并作废。
A44_KEYS = {"weight_mode", "legacy_allow_weight_fallback", "algorithm_weight_mode"}
# §9.68 已退役的逐帧形态判别键（normalize 侧由 §9.68 删除；mosaic/export 侧按 §9.71 定案 4 保留在旧分支）。
RETIRED_NORMALIZE_KEYS = {"phase_name", "config", "inputs"}
# 块面禁现的「逐帧形态」键（§3.3：命令名就是阶段身份；滤镜身份由 HiPS properties.obs_filter 承载）。
FORBIDDEN_IN_NEW_BRANCH = {"phase_name", "config", "inputs", "product", "filter"}
# normalize 块内**既存**的 12 个会话可选键（在 parser.cpp session_keys() 平铺白名单内、不在
# config_fields() 内 ⇒ CLI 的块内未知键门判 rc=3）。本表是**登记差异**（只减不增）：
# 收口二选一（均不在本任务文件域）——① 补进 config_fields()（lib/infrastructure/cli/**）；
# ② 从 normalize_block 移除并改指 config_registry.json 的两条 sparse_snr_layer 登记点（eng/packaging/config/**）。
NORMALIZE_BLOCK_EXTRA_KEYS = {
    "cosmetic", "dark_optimization", "dark_scale_factor", "master_units", "master_scale",
    "master_flat_normalize", "master_flat_median_range", "photometry", "sparse_snr_layer",
    "sparse_snr_spacing_px", "algorithm_drizzle_pixfrac", "algorithm_psf_model",
}


def messages(errors):
    return " | ".join("%s: %s" % ("/".join(str(p) for p in path) or "<root>", msg)
                      for path, msg in errors)


def errs_for(phase, doc):
    return C.validate(C.load_json(PHASE_SCHEMAS[phase]), doc)


def _cfg_vector_body(var):
    text = C.load_text(SESSION_H)
    m = re.search(r"static const std::vector<ConfigField>\s+%s\s*=\s*\{(.*?)\n    \};" % var,
                  text, re.S)
    assert m, "session_commands.h 中找不到 %s（唯一键集声明已漂移）" % var
    return m.group(1)


def cli_config_fields():
    """从 session_commands.h 现场派生 {会话: {键: 是否有模板值}}（唯一声明的机器读取）。"""
    out = {}
    for phase, var in (("normalize", "kNormalize"), ("mosaic", "kMosaic"), ("export", "kExport")):
        body = _cfg_vector_body(var)
        fields = {}
        for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(nullptr|")', body):
            key, second = m.group(1), m.group(2)
            # 点号说明键（如 wcs.init_source）落到其根键（JSON 里就是 wcs 对象）
            root = key.split(".")[0]
            has_json = second == '"'
            fields[root] = fields.get(root, False) or has_json
        assert fields, "%s 键集解析为空（解析器失效，不得判绿）" % var
        out[phase] = fields
    return out


def cli_block_keys():
    """parser.cpp block_keys()（块级键唯一声明）。"""
    text = C.load_text(PARSER_CPP)
    m = re.search(r"block_keys\(\)\s*\{.*?=\s*\{([^}]*)\}", text, re.S)
    assert m, "parser.cpp 找不到 block_keys()"
    return set(re.findall(r'"([^"]+)"', m.group(1)))


def parser_session_keys():
    """parser.cpp session_keys()：**平铺形态**顶层键白名单（全局并集）。"""
    text = C.load_text(PARSER_CPP)
    m = re.search(r"session_keys\(\)\s*\{.*?=\s*\{(.*?)\n    \};", text, re.S)
    assert m, "parser.cpp 找不到 session_keys()"
    keys = set(re.findall(r'"([^"]+)"', m.group(1)))
    assert keys, "session_keys() 解析为空（解析器失效，不得判绿）"
    return keys


# §3.3「平铺单块简写与 blocks[] 等价」的**现实差异**（本任务实测，只减不增）：
# 平铺门 session_keys() 是全局并集（第二份键名清单），比 ⋃_sessions config_fields() 多出这些键
# ⇒ 平铺形态收得进、blocks[] 内判 unknown key（rc=3），两形态**不等价**。
# 收口配方（二选一，均在 lib/**，不在本任务文件域）：
#   ① 按会话把这 19 键补进 config_fields()（json=nullptr ⇒ 不进 --template/--help 骨架，
#      但平铺/块内两门同时认它）；② 从 session_keys() 删除（仅当该键确无生产消费者）。
# 收口后本表必须清空，断言随之变严（== set()）。
FLAT_ONLY_RESIDUE = {
    "algorithm_psf_model", "algorithm_upm_gauge", "cosmetic", "dark_optimization",
    "dark_scale_factor", "frame", "master_flat_median_range", "master_flat_normalize",
    "master_scale", "master_units", "max_tiles", "mode", "output_fits_path",
    "persist_upm", "photometry", "reject_profile", "sampler_used", "sparse_snr_layer",
    "upm_save_path",
}


def expected_block_keys(phase):
    fields = cli_config_fields()[phase]
    keys = {k for k in fields if k not in A44_KEYS and k != "schema_version"}
    return keys | cli_block_keys()


def _resolve_pointer(schema, pointer):
    """按 CFG002 口径解析登记点（支持 properties/blocks[] 的 [] 尾缀与本地 $ref 展开）。"""
    node = schema
    parts = [p for p in pointer.lstrip("#").split("/") if p]
    for i, part in enumerate(parts):
        if part.endswith("[]"):
            part = part[:-2]
            if not isinstance(node, dict) or part not in node:
                return None
            node = node[part]
            node = node.get("items", node) if isinstance(node, dict) else node
        else:
            if not isinstance(node, dict) or part not in node:
                return None
            node = node[part]
        for _ in range(3):
            if isinstance(node, dict) and "$ref" in node and str(node["$ref"]).startswith("#/"):
                node = _resolve_pointer(schema, node["$ref"][1:])
            else:
                break
        if node is None:
            return None
    return node


def block_props(schema, phase):
    return set(schema["$defs"]["%s_block" % phase]["properties"])


def blocks_items(schema, phase):
    """blocks[] 项定义（三份 schema 同构：顶层 properties.blocks → $defs/<phase>_block）。"""
    return schema["properties"]["blocks"]["items"]


def flat_branch(schema):
    """平铺单块简写分支：mosaic/export 在 else.else（else.then 是旧合同分支）；normalize 在 else。"""
    outer = schema["else"]
    return outer["else"] if "if" in outer else outer


def legacy_branch(schema):
    outer = schema["else"]
    return outer["then"] if "if" in outer else None


class TestUnifiedBranchShape(unittest.TestCase):
    def test_01_three_schemas_expose_the_same_three_branches(self):
        """三分支同构：blocks[]（then）/ 旧合同（else.then）/ 平铺单块简写（else.else）。"""
        for phase, rel in PHASE_SCHEMAS.items():
            s = C.load_json(rel)
            self.assertEqual(["blocks"], s["if"]["required"], "%s 形态判别必须只看 blocks" % rel)
            then = s["then"]
            self.assertEqual(["schema_version", "blocks"], then["propertyNames"]["enum"])
            self.assertEqual({"$ref": "#/$defs/%s_block" % phase}, blocks_items(s, phase),
                             "%s 的 blocks[] 项必须指向 $defs/%s_block（单一定义）" % (rel, phase))
            flat = flat_branch(s)
            self.assertIn("schema_version", flat["required"])
            self.assertIn(INPUT_KEY[phase], flat["required"],
                          "%s 平铺简写必须要求该会话的输入帧键" % rel)
            self.assertIn("output_dir", flat["required"])
            self.assertNotIn("name", flat["propertyNames"]["enum"],
                             "%s 平铺简写不含块标签 name（CLI 平铺门亦不收）" % rel)
            # 平铺简写 = 一个块去掉 name（§3.3「与多块形态等价」）
            self.assertEqual(block_props(s, phase) - {"name"},
                             set(flat["propertyNames"]["enum"]) - {"schema_version"},
                             "%s 平铺简写键集 != 块内键集 − name（两形态不等价）" % rel)
            self.assertEqual(sorted(flat["propertyNames"]["enum"]),
                             sorted(["schema_version"] + sorted(block_props(s, phase) - {"name"})),
                             "%s 平铺简写键集必须与块内键集逐项一致（禁止第二份键名清单）" % rel)

    def test_02_legacy_branch_retention_is_registered(self):
        """§9.71 裁决 2 定案 4：mosaic/export 旧合同分支**保留**；normalize 由 §9.68 退役。"""
        for phase in ("mosaic", "export"):
            s = C.load_json(PHASE_SCHEMAS[phase])
            legacy = legacy_branch(s)
            self.assertEqual(["phase_name", "config", "inputs"], legacy["required"])
            self.assertEqual(phase, legacy["properties"]["phase_name"]["const"])
            self.assertEqual("#/$defs/%s_config" % phase, legacy["properties"]["config"]["$ref"])
            for leaf in ("%s_config" % phase, "%s_inputs" % phase, "filter_name", "precision", "output_dir"):
                self.assertIn(leaf, s["$defs"], "%s 旧合同 $defs.%s 不得删除（登记点依赖）" % (phase, leaf))
            for rel in (POS + "%s_legacy_contract.phase_config.json" % phase,):
                self.assertEqual([], C.validate(s, C.load_json(rel)),
                                 "%s 旧合同分支必须仍通过 schema（不退役）" % rel)
        n = C.load_json(PHASE_SCHEMAS["normalize"])
        self.assertIsNone(legacy_branch(n),
                          "normalize 无旧合同分支（§9.68 退役；不得以任何形态复活）")
        for key in sorted(RETIRED_NORMALIZE_KEYS):
            doc = C.load_json(POS + "normalize_blocks.phase_config.json")
            doc[key] = "mosaic" if key == "phase_name" else {}
            self.assertTrue(C.validate(n, doc), "normalize 顶层 %s 必须判红（§9.68 退役形态）" % key)
        self.assertNotIn("phase_name", C.property_names(n),
                         "normalize 不得复活逐帧形态的 phase_name 属性（§9.68）")
        for leaf in ("normalize_config", "normalize_inputs"):
            self.assertNotIn(leaf, n["$defs"], "normalize 旧 $defs.%s 已随 §9.68 删除" % leaf)


class TestBlockKeySetSingleSource(unittest.TestCase):
    def test_03_block_keys_equal_cli_single_declaration(self):
        """块内键集 = config_fields() + block_keys()（禁止第二份键名清单）。"""
        self.assertEqual({"name", "output_dir"}, cli_block_keys(),
                         "parser.cpp block_keys() 变化必须同步本测试与 schema")
        for phase in ("mosaic", "export"):
            s = C.load_json(PHASE_SCHEMAS[phase])
            self.assertEqual(expected_block_keys(phase), block_props(s, phase),
                             "%s 块内键集与 CLI 单一声明不一致（多/少键都会让用户拿到 rc=3）" % phase)
        n = C.load_json(PHASE_SCHEMAS["normalize"])
        got = block_props(n, "normalize")
        self.assertTrue(expected_block_keys("normalize") <= got,
                        "normalize 块内键集必须覆盖 CLI 单一声明：缺 %s"
                        % sorted(expected_block_keys("normalize") - got))
        self.assertEqual(NORMALIZE_BLOCK_EXTRA_KEYS, got - expected_block_keys("normalize"),
                         "normalize 块内既存差异集变化（只减不增；收口须同步 config_registry.json 登记点）")

    def test_03b_key_set_derivation_has_teeth(self):
        """能红：故意多一个键/少一个键都必须被同一条判据抓住（防判据恒真）。"""
        s = C.load_json(PHASE_SCHEMAS["mosaic"])
        base = block_props(s, "mosaic")
        self.assertNotEqual(expected_block_keys("mosaic"), base | {"bogus_knob"})
        self.assertNotEqual(expected_block_keys("mosaic"), base - {"hips_paths"})
        self.assertIn("hips_paths", cli_config_fields()["mosaic"], "解析器必须真的读到键名")
        self.assertTrue(any(v for v in cli_config_fields()["mosaic"].values()),
                        "模板值判定（json != nullptr）必须真的区分出骨架键")

    def test_04_a44_keys_excluded_from_new_branches(self):
        """§9.73 裁决 A44：三份 schema **任何分支**都不收 weight_mode / legacy_allow_weight_fallback / algorithm_weight_mode。

        config 面收口（本任务同批）：config_registry.json 的 weight_mode 登记注销、
        eng/packaging/config/defaults.json#weight.default_mode 组删除、phase_config_mosaic 旧合同的
        algorithm_weight_mode 属性删除（否则合同仍祝福一个「不存在」的键）。
        """
        for phase, rel in PHASE_SCHEMAS.items():
            s = C.load_json(rel)
            self.assertEqual(set(), A44_KEYS & block_props(s, phase), "%s 块面出现 A44 键" % rel)
            self.assertEqual(set(), A44_KEYS & set(flat_branch(s)["propertyNames"]["enum"]),
                             "%s 平铺面出现 A44 键" % rel)
            self.assertEqual(set(), A44_KEYS & C.property_names(s),
                             "%s 出现 A44 键属性（任何分支都不允许）" % rel)
        # 登记面与默认值面同批无残留（唯一事实源不得再登记不存在的键）
        reg = C.load_json(REGISTRY)
        self.assertFalse([r for r in reg["plugin_knobs"]
                          if "algorithm_weight_mode" in json.dumps(r, ensure_ascii=False)],
                         "config_registry.json 仍登记 algorithm_weight_mode")
        defaults = C.load_json(DEFAULTS)
        self.assertFalse([f for f in defaults["fields"]
                          if "weight.default_mode" in json.dumps(f, ensure_ascii=False)],
                         "defaults.json 仍登记 weight.default_mode")
        self.assertEqual(len(defaults["fields"]), defaults["field_count"],
                         "defaults.json field_count 与实际字段数不一致")
        for phase, rel in PHASE_SCHEMAS.items():
            s = C.load_json(rel)
            for key in sorted(A44_KEYS):
                doc = C.load_json(POS + "%s_blocks.phase_config.json" % phase)
                doc["blocks"][0][key] = 1
                self.assertTrue(C.validate(s, doc), "%s 块内 %s 必须判红（A44）" % (phase, key))
        errs = errs_for("mosaic", C.load_json(NEG + "mosaic_block_weight_mode.phase_config.json"))
        self.assertTrue(errs, "A44 负例夹具必须判红")
        self.assertIn("weight_mode", messages(errs))

    def test_05_minmax_rejection_path_is_deleted(self):
        """§9.71 裁决 3 + DESIGN-DRAFT §1.8：min/max 禁用 ⇒ 枚举里不得再有 minmax。"""
        for phase, rel in PHASE_SCHEMAS.items():
            s = C.load_json(rel)
            enums = []

            def walk(node):
                if isinstance(node, dict):
                    if "enum" in node:
                        enums.append(node["enum"])
                    for v in node.values():
                        walk(v)
                elif isinstance(node, list):
                    for v in node:
                        walk(v)

            walk(s)
            for e in enums:
                self.assertNotIn("minmax", [str(v) for v in e],
                                 "%s 的枚举仍含 minmax（排异 min/max 必须禁用）" % rel)
        legacy = C.load_json(PHASE_SCHEMAS["mosaic"])["$defs"]["mosaic_config"]["properties"]["algorithm_rejection_method"]
        self.assertEqual(["auto", "sigma", "winsorized", "averaged_sigma", "linear_fit", "esd", "percentile"],
                         legacy["enum"])

    def test_06_new_branches_have_no_phase_or_per_frame_keys(self):
        """§3.3：新分支不出现 phase_name / config / inputs[].product / inputs[].filter。"""
        for phase, rel in PHASE_SCHEMAS.items():
            s = C.load_json(rel)
            self.assertEqual(set(), FORBIDDEN_IN_NEW_BRANCH & block_props(s, phase), rel)
            self.assertEqual(set(), FORBIDDEN_IN_NEW_BRANCH & set(flat_branch(s)["propertyNames"]["enum"]), rel)
        for fixture in ("mosaic_blocks_mixed_flat", "export_blocks_mixed_flat",
                        "mosaic_block_weight_mode", "export_block_phase_name"):
            phase = fixture.split("_")[0]
            errs = errs_for(phase, C.load_json(NEG + fixture + ".phase_config.json"))
            self.assertTrue(errs, "负例 %s 必须判红" % fixture)


class TestTemplatesShareTheSingleKeyTable(unittest.TestCase):
    def test_07_templates_are_block_form_and_schema_valid(self):
        for phase, rel in TEMPLATES.items():
            tpl = C.load_json(rel)
            self.assertIn("blocks", tpl, "%s 必须是块形态（§9.71 裁决 2）" % rel)
            self.assertTrue(tpl["blocks"], "%s blocks 不得为空" % rel)
            self.assertNotIn("phase_name", tpl, "%s 不得再是旧合同形态（旧形态是合同留痕，不是 CLI 运行形态）" % rel)
            self.assertEqual([], C.validate(C.load_json(PHASE_SCHEMAS[phase]), tpl),
                             "%s 未通过自身 schema" % rel)

    def test_08_template_keys_come_from_the_single_key_table(self):
        """模板键 ⊆ CLI 块内键集（单一键表）；块级必填键齐；CLI 骨架键（json != nullptr）一个不缺。

        §9.71 裁决 2 定案 3「键名一律以命令行实际认的键为准」⇒ 模板**不得**自造键，也**不得**
        省略 CLI 骨架键（FIX-203 已把 snr_path / rotation_deg / crpix_px 落进键表，故无豁免）。
        """
        for phase, rel in TEMPLATES.items():
            s = C.load_json(PHASE_SCHEMAS[phase])
            tpl = C.load_json(rel)
            keys = set()
            for blk in tpl["blocks"]:
                keys |= set(blk.keys())
            self.assertTrue(keys <= block_props(s, phase),
                            "%s 模板出现 CLI 不认的键 %s（模板将不可运行）"
                            % (rel, sorted(keys - block_props(s, phase))))
            self.assertTrue(set(s["$defs"]["%s_block" % phase]["required"]) <= keys,
                            "%s 模板缺块级必填键 %s"
                            % (rel, sorted(set(s["$defs"]["%s_block" % phase]["required"]) - keys)))
            skeleton = {k for k, has in cli_config_fields()[phase].items()
                        if has and k != "schema_version"} | {"name"}
            self.assertEqual(set(), skeleton - keys,
                             "%s 模板漏了 CLI 骨架键 %s（模板与键表必须同源，不得省略）"
                             % (rel, sorted(skeleton - keys)))

    def test_11_flat_and_block_forms_share_one_per_session_key_table(self):
        """§3.3：平铺单块简写与 blocks[] **等价** ⇒ CLI 两道门必须同源于一份按会话键表。

        现状（本任务实测）：块内门 = config_fields(session) + block_keys()（按会话，正确）；
        平铺门 = session_keys() 全局并集（第二份键名清单，偏宽）。差异**精确登记**在
        FLAT_ONLY_RESIDUE（只减不增），本测试锁死「差异 = 登记表」这一事实：
        差异变大 ⇒ 判红；收口后 ⇒ 登记表清空、断言变严。
        """
        flat = parser_session_keys()
        per_session = set()
        for phase in PHASE_SCHEMAS:
            per_session |= set(cli_config_fields()[phase])
        self.assertEqual(FLAT_ONLY_RESIDUE, flat - per_session,
                         "平铺门 − ⋃config_fields 的差异必须与登记表逐字一致（新增即判红）：多=%s 少=%s"
                         % (sorted((flat - per_session) - FLAT_ONLY_RESIDUE),
                            sorted(FLAT_ONLY_RESIDUE - (flat - per_session))))
        # 反向：config_fields 的键必须全部被平铺门接受，否则平铺面比块面**窄**（另一种不等价）。
        # schema_version 由两形态各自单列（parser.cpp session_blocks_errors 显式跳过），故排除。
        loose = per_session - flat - cli_block_keys() - {"schema_version"}
        self.assertEqual(set(), loose,
                         "config_fields 的键必须被平铺门接受（块面不得比平铺面宽）：%s" % sorted(loose))
        # schema 面已经等价：平铺分支键集 == 块内键集 − {name}（差异只许留在 CLI 实现面）
        for phase, rel in PHASE_SCHEMAS.items():
            s = C.load_json(rel)
            self.assertEqual(block_props(s, phase) - {"name"},
                             set(flat_branch(s)["propertyNames"]["enum"]) - {"schema_version"},
                             "%s schema 的平铺分支与块内键集必须等价" % rel)

    def test_09_export_projection_enum_matches_the_registry(self):
        """新分支投影码 = 投影注册表冻结集（单一口径，禁止第二份投影词表）。"""
        reg = C.load_json(PROJ_REGISTRY)
        frozen = reg["properties"]["frozen_set"]["items"]["enum"]
        codes = reg["properties"]["projections"]["items"]["properties"]["code"]["enum"]
        self.assertEqual(frozen, codes)
        block = C.load_json(PHASE_SCHEMAS["export"])["$defs"]["export_block"]["properties"]["projection"]
        self.assertEqual(frozen, block["$ref"] and C.load_json(PHASE_SCHEMAS["export"])["$defs"]["projection_code"]["enum"])

    def test_10_registry_and_defaults_pointers_still_resolve(self):
        """旧合同分支保留的机器理由：登记点与 enum_target 指针必须继续可解析。"""
        schemas = {rel: C.load_json(rel) for rel in PHASE_SCHEMAS.values()}
        reg = C.load_json(REGISTRY)
        checked = 0
        for r in reg["plugin_knobs"]:
            at = r.get("registered_at")
            if not isinstance(at, str) or "#" not in at:
                continue
            path, pointer = at.split("#", 1)
            if path not in schemas:
                continue
            node = _resolve_pointer(schemas[path], pointer)
            self.assertIsNotNone(node, "指针不可解析: %s" % at)
            checked += 1
        self.assertGreaterEqual(checked, 8, "phase_config 登记点数量异常（%d）" % checked)
        for f in C.load_json(DEFAULTS)["fields"]:
            tgt = f.get("enum_target")
            if not tgt:
                continue
            schema = C.load_json(tgt["schema"])
            node = _resolve_pointer(schema, tgt["pointer"])
            self.assertIsInstance(node, dict, "enum_target 指针不可解析: %s" % tgt)
            self.assertIn("enum", node, "enum_target 必须落到含 enum 的节点: %s" % tgt)
            self.assertIn(str(f["enum_token"]), [str(v) for v in node["enum"]],
                          "enum_token 必须落在目标 enum 内: %s" % f["key"])

    def test_11_flat_shorthand_positives_pass(self):
        for phase in ("mosaic", "export"):
            rel = POS + "%s_flat.phase_config.json" % phase
            self.assertEqual([], errs_for(phase, C.load_json(rel)),
                             "%s 平铺单块简写必须合法（§3.3）" % rel)

    def test_12_mixed_forms_are_rejected_with_branch_diagnostics(self):
        for phase in ("mosaic", "export"):
            errs = errs_for(phase, C.load_json(NEG + "%s_blocks_mixed_flat.phase_config.json" % phase))
            self.assertTrue(errs, "%s：blocks 与平铺键同时出现必须判红（不静默取一）" % phase)
            self.assertIn("blocks", messages(errs))


if __name__ == "__main__":
    unittest.main(verbosity=2)