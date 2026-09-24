#!/usr/bin/env python3
"""SNR 口径门：Phase1 的 SNR 产品必须自报「是绝对 SNR 还是无增益上界」。

为什么需要这道门
----------------
最高设计 §2.2 的交付物是**绝对 SNR**（PSF 信号 SNR）。但 sigma_F 的源光子散粒项
F*P_i/g 含 1/g，**增益未知时该项不可计算**，sigma_F 只剩天空项，SNR 系统性偏高
（SNR_rep/SNR_true = sqrt(1 + F/sigma_bg^2)）。若不把口径写进产品，消费者会把
无增益上界当成绝对 SNR 用——下游定权 w = SNR^2/F_ref^2 直接受此影响。

判据（能红能绿）
----------------
W1 产品契约：逐帧 snr_caliber 必填；与 snr_source_shot_term_included /
   snr_gain_e_per_adu 三者自洽；upper_bound_no_gain 必带非空 snr_degraded_reason；
   顶层 snr_caliber 必须等于逐帧聚合。
W2 文档约束：07_noise_snr.md 决策树第 3 条必须写明「源泊松项不可加」+ 上界标记要求。
W3 代码事实：module_adapters.cpp 必须写出 snr_caliber 与 snr_degraded_reason。

用法：python3 check_snr_caliber.py [p1_snr.json ...]      # 无参数则只跑静态面
      python3 check_snr_caliber.py --self-test            # 正/负例自证
"""
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[5]
DOC = REPO / "docs" / "plugins" / "algorithms_phase1" / "07_noise_snr.md"
IMPL = REPO / "lib" / "infrastructure" / "scheduler" / "src" / "module_adapters.cpp"

CAL_ABS = "absolute_flux_type_snr"
CAL_UB = "upper_bound_no_gain"
CAL_MIXED = "mixed_or_unavailable"


def check_product(doc, where):
    """W1：单份 p1_snr.json 的口径自洽性。返回 findings 列表。"""
    out = []
    frames = doc.get("frames")
    if not isinstance(frames, list):
        return [(where, "frames 不是数组")]
    n_abs = n_ub = 0
    for i, f in enumerate(frames):
        tag = "%s frames[%d]" % (where, i)
        cal = f.get("snr_caliber")
        if cal is None:
            out.append((tag, "缺 snr_caliber：产品未自报 SNR 口径，"
                        "消费者无法区分绝对 SNR 与无增益上界"))
            continue
        inc = f.get("snr_source_shot_term_included")
        gain = f.get("snr_gain_e_per_adu")
        if cal == CAL_ABS:
            n_abs += 1
            if inc is not True:
                out.append((tag, "caliber=绝对但 snr_source_shot_term_included!=true"))
            if not (isinstance(gain, (int, float)) and gain > 0):
                out.append((tag, "caliber=绝对但 snr_gain_e_per_adu 非正/缺失"))
        elif cal == CAL_UB:
            n_ub += 1
            if inc is not False:
                out.append((tag, "caliber=上界但 snr_source_shot_term_included!=false"))
            if gain is not None:
                out.append((tag, "caliber=上界但 snr_gain_e_per_adu 非 null"))
            dr = f.get("snr_degraded_reason")
            if not (isinstance(dr, str) and dr.strip()):
                out.append((tag, "caliber=上界但 snr_degraded_reason 为空（静默降级）"))
        else:
            out.append((tag, "snr_caliber 取值不在词表内: %r" % (cal,)))
    top = doc.get("snr_caliber")
    expect = (CAL_ABS if (n_abs > 0 and n_ub == 0) else
              CAL_UB if (n_ub > 0 and n_abs == 0) else CAL_MIXED)
    if top != expect:
        out.append((where, "顶层 snr_caliber=%r 与逐帧聚合 %r 不一致" % (top, expect)))
    if doc.get("n_frames_absolute_snr") != n_abs or doc.get("n_frames_upper_bound_no_gain") != n_ub:
        out.append((where, "顶层口径计数与逐帧不符"))
    return out


def check_static():
    """W2 + W3：文档正向约束与实现事实。"""
    out = []
    if not DOC.is_file():
        return [("W2", "文档不存在: %s" % DOC)]
    txt = DOC.read_text(encoding="utf-8")
    if "源泊松项" not in txt or "不可加" not in txt:
        out.append(("W2-DOC", "决策树未写明 gain 未知时源泊松项不可加"))
    if CAL_UB not in txt:
        out.append(("W2-DOC", "文档未要求产品标 %s" % CAL_UB))
    if "上界" not in txt:
        out.append(("W2-DOC", "文档未声明无增益口径的 SNR 是上界"))
    if not IMPL.is_file():
        out.append(("W3", "实现文件不存在: %s" % IMPL))
        return out
    code = IMPL.read_text(encoding="utf-8")
    for key in ("snr_caliber", "snr_degraded_reason", "snr_source_shot_term_included"):
        if key not in code:
            out.append(("W3-IMPL", "实现未写出 %s" % key))
    if CAL_UB not in code:
        out.append(("W3-IMPL", "实现未使用口径词表项 %s" % CAL_UB))
    return out


def self_test():
    """正/负例自证：判据必须能红。"""
    fails = []
    ok_ub = {"frames": [{"snr_caliber": CAL_UB, "snr_source_shot_term_included": False,
                        "snr_gain_e_per_adu": None, "snr_degraded_reason": "gain_unknown"}],
             "snr_caliber": CAL_UB, "n_frames_absolute_snr": 0,
             "n_frames_upper_bound_no_gain": 1}
    ok_abs = {"frames": [{"snr_caliber": CAL_ABS, "snr_source_shot_term_included": True,
                          "snr_gain_e_per_adu": 1.3, "snr_degraded_reason": None}],
              "snr_caliber": CAL_ABS, "n_frames_absolute_snr": 1,
              "n_frames_upper_bound_no_gain": 0}
    for name, d in (("正例-上界", ok_ub), ("正例-绝对", ok_abs)):
        r = check_product(d, name)
        if r:
            fails.append("%s 被误判红: %s" % (name, r))

    neg = []
    d = json.loads(json.dumps(ok_ub)); d["frames"][0].pop("snr_caliber")
    neg.append(("负例1 缺 caliber", d))
    d = json.loads(json.dumps(ok_ub)); d["frames"][0]["snr_degraded_reason"] = ""
    neg.append(("负例2 上界无降级原因", d))
    d = json.loads(json.dumps(ok_abs)); d["frames"][0]["snr_source_shot_term_included"] = False
    neg.append(("负例3 绝对口径却未含源项", d))
    d = json.loads(json.dumps(ok_ub)); d["snr_caliber"] = CAL_ABS
    neg.append(("负例4 顶层与逐帧不一致", d))
    d = json.loads(json.dumps(ok_ub)); d["frames"][0]["snr_gain_e_per_adu"] = 1.3
    neg.append(("负例5 上界却带增益", d))
    for name, d in neg:
        if not check_product(d, name):
            fails.append("%s 未被判红（判据恒真）" % name)

    if fails:
        print("SNR_CALIBER_SELFTEST_FAIL:")
        for x in fails:
            print("  " + x)
        return 1
    print("SNR_CALIBER_SELFTEST_PASS: 两种口径正例均绿；缺 caliber / 上界无降级原因 / "
          "绝对却无源项 / 顶层不一致 / 上界带增益 五条负例均红")
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    findings = check_static()
    for p in argv:
        path = Path(p)
        if not path.is_file():
            findings.append((p, "文件不存在"))
            continue
        try:
            findings.extend(check_product(json.loads(path.read_text(encoding="utf-8")), str(path)))
        except Exception as exc:  # noqa: BLE001
            findings.append((p, "解析失败: %s" % exc))
    if findings:
        print("SNR_CALIBER_GATE_FAIL")
        for cid, msg in findings:
            print("  [%s] %s" % (cid, msg))
        return 1
    print("SNR_CALIBER_GATE_PASS: 文档约束 + 实现事实 + %d 份产品口径自洽" % len(argv))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
