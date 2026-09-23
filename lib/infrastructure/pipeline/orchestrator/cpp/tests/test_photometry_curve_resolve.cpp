// ============================================================================
// test_photometry_curve_resolve.cpp - 滤镜/QE 响应曲线解析「能红能绿」门
//                                      （PHOTOCURVE-ORCH-01）
// ----------------------------------------------------------------------------
// 被测面 = **生产唯一实现** lib/algorithms/photometry/cpp/src/filter_curve_json.h
//   （orchestrator 的 run_stage_photometric 与 frame_photometry_fit 都只做委托）。
// 规范依据:
//   - ENGINEERING_SPEC.md:175「每项检查有正例与负例（能红能绿）」；
//   - docs/standards/CODE_STANDARD.md §MUST「禁止重复 production science
//     implementation（单一实现 + oracle）」+「禁止 silent config fallback 改变
//     科学语义」；
//   - eng/packaging/config/filters.json#lookup.resolution_rule 逐字:
//     「resolve(name) = filters[name] if name in keys(filters) else
//       ERROR(unknown_filter)」+ 同文件 lookup.non_key_examples（"bader r" 等
//     非库键串的 resolution = ERROR）；文档化说明 docs/contracts/CONFIG_CONTRACT.md §4。
// 判据（逐条可红）:
//   [G1] 绿: 转录版 filters.json + "Baader R" ⇒ 73 点 / [572,716] nm
//   [G2] 绿: 原始版 filters.json + "Baader R" ⇒ 73 点 / [572,716] nm（无回归）
//   [R1] 红对照: 修复前口径 + 转录版 ⇒ 53 点 / [420,524] nm（= Antlia V Pro Series B）
//   [R2] 红对照: 修复前口径 + 原始版 ⇒ 73 点（缺陷只在转录版触发）
//   [N1] 负例: 不存在的曲线名 ⇒ 正确拒绝（两版都判）
//   [N2] 负例: 库内登记的非键反例 "bader r" ⇒ 正确拒绝；修复前口径会**静默接受**
//   [N3] 负例: provenance 有名字、filters 段没有 ⇒ kCurveNameOutsideLibrary（显式报错）
//   [N4] 非退化对照: 同一夹具下修复前口径**静默取到 filters 段第一条曲线**
//   [I0..I9] 通带身份门（装配期 fail-closed, PASSBAND-IDENTITY-GATE-01）:
//     绿（声明通带 ⇒ 成功 + 身份自述 = 实际曲线）/ 负例（对象自述名不符、
//     对象整段换成别支滤镜、provenance 声明不符、provenance 声明对调、
//     filters 段重复键 + 错曲线、provenance 缺条目）/ 恒真自检（正例与全部
//     错误输入的判定必须互不相同）/ 接线（生产拟合入口具名上报 + 声明通带核对）
//   [W1..W2] 接线: 两个调用方 TU 都只调唯一实现，且不再含修复前解析器标记
// 本 TU 不自持任何 I/O 原语（读文件一律经 aio_file::read_all）⇒ 不触发
// AIO-IO-BOUNDARY 台账 N2「未登记文件出现命中」。
// 用法: test_photometry_curve_resolve <repo_root> [--fault-inject=legacy]
//   --fault-inject=legacy: 把 [G1] 的装载口径替换为修复前的文本搜索（故障注入），
//   期望 [G1] 判红并输出 FAULT_INJECT(legacy) DETECTED（rc=0 = 注入被检出 ⇒ 本门
//   有判别力；rc=1 = 注入未被检出 ⇒ 门本身失效）。依据 ENGINEERING_SPEC.md:176
//   「每项检查提供机器可执行负例入口（--self-test 或 --fault-inject）」。
// ============================================================================

#include "filter_curve_json.h"   // 生产唯一实现（被测面）
#include "aio_file_io.h"         // aio_file::read_all / RandomWriter（aio 唯一 I/O 实现）
#include "aio_atomic_file.h"     // aio_atomic::make_dirs / remove_file（文件系统原语）

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace cj = astrocs::photometry::curve_json;

namespace {

int g_fail = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", what.c_str());
    if (!ok) ++g_fail;
}

std::string curve_repr(const std::vector<double>& wl) {
    if (wl.empty()) return "n=0 []";
    double lo = wl.front(), hi = wl.front();
    for (double v : wl) { lo = std::min(lo, v); hi = std::max(hi, v); }
    char buf[160];
    std::snprintf(buf, sizeof(buf), "n=%zu [%.1f,%.1f]", wl.size(), lo, hi);
    return std::string(buf);
}

// 读文件（经 aio；本 TU 无文件系统原语）。失败 ⇒ 空串。
std::string slurp(const std::string& path) {
    std::string s;
    if (!aio_file::read_all(path.c_str(), &s)) return std::string();
    return s;
}

// 写变异体（经 aio 唯一 I/O 边界：aio_atomic::make_dirs + aio_file::RandomWriter）。
// 身份门是**装配检查**：判据的输入就是"库文件文本"本身，故负例必须落在磁盘上
// （不能只改内存字符串）。
bool write_file(const std::string& path, const std::string& content) {
    const std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0)
        aio_atomic::make_dirs(path.substr(0, slash));
    aio_file::RandomWriter w;
    std::string err;
    if (!w.open(path.c_str(), &err)) return false;
    if (!w.write_at(0, content.data(), content.size(), &err)) return false;
    return w.flush_and_sync(&err);
}

void remove_file(const std::string& path) { aio_atomic::remove_file(path); }

// 把 "[a, b, c]" 形式的数值数组按固定小数位重排（构造**等长但数值不同**的变异体；
// 纯文本操作，不引入第二个 JSON 解析实现）。
std::string retune_number_array(const std::string& arr, double delta, int decimals) {
    if (arr.size() < 2 || arr.front() != '[' || arr.back() != ']') return std::string();
    std::string body = arr.substr(1, arr.size() - 2);
    for (char& c : body) {
        if (c == ',') c = ' ';
    }
    std::istringstream iss(body);
    std::string out = "[";
    double v = 0.0;
    bool first = true;
    while (iss >> v) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, v + delta);
        if (!first) out += ", ";
        out += buf;
        first = false;
    }
    out += "]";
    return first ? std::string() : out;   // 一个数都没解析出来 ⇒ 失败
}

// ── 仅负例对照: 修复前的「文本搜索」口径 ────────────────────────────────────
// 解析口径逐字取自修复前的
//   lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1368-1421
//   （git HEAD, 与 frame_photometry_fit.cpp:60-91 修复前同源）。
// **禁止用于生产**；它存在的唯一理由是让本门能红：若生产实现被改回该口径，
// [R1]/[N2]/[N4] 立即判红。读取部分改由调用方提供（本 TU 不得自持 I/O 原语），
// 解析分支与判据逐字保留。
bool legacy_text_search_control(const std::string& content,
                                const std::string& filter_name,
                                std::vector<double>& out_wl,
                                std::vector<double>& out_trans) {
    std::string key = "\"" + filter_name + "\"";
    size_t pos = content.find(key);
    if (pos == std::string::npos) return false;
    auto extract_array = [&content, &pos](const std::string& arr_key,
                                           std::vector<double>& out) -> bool {
        size_t kpos = content.find(arr_key, pos);
        if (kpos == std::string::npos) return false;
        size_t bracket_start = content.find('[', kpos);
        if (bracket_start == std::string::npos) return false;
        size_t bracket_end = content.find(']', bracket_start);
        if (bracket_end == std::string::npos) return false;
        std::string arr_str = content.substr(bracket_start + 1,
                                              bracket_end - bracket_start - 1);
        std::replace(arr_str.begin(), arr_str.end(), ',', ' ');
        std::istringstream iss(arr_str);
        out.clear();
        double v;
        while (iss >> v) out.push_back(v);
        return !out.empty();
    };
    if (!extract_array("\"wavelength_nm\"", out_wl) ||
        !extract_array("\"value\"", out_trans)) {
        return false;
    }
    return out_wl.size() == out_trans.size();
}

}  // namespace

int main(int argc, char** argv) {
    const std::string repo = (argc > 1) ? argv[1] : ".";
    const bool inject_legacy =
        (argc > 2 && std::string(argv[2]) == "--fault-inject=legacy");
    const std::string transcribed = repo + "/eng/packaging/config/filters.json";
    const std::string original =
        repo + "/lib/algorithms/photometry/data/response_curves/filters.json";
    const std::string fixture =
        repo + "/lib/infrastructure/pipeline/orchestrator/cpp/tests/fixtures/"
               "filters_provenance_name_not_in_library.json";
    const std::string orch_src =
        repo + "/lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp";
    const std::string fit_src =
        repo + "/lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp";

    std::printf("PHOTOCURVE-ORCH-01 曲线解析门 (repo=%s)\n", repo.c_str());

    // ── 绿: 生产唯一实现 ────────────────────────────────────────────────────
    {
        std::vector<double> wl, tr;
        cj::LoadStatus st = cj::LoadStatus::kCurveNotFound;
        if (inject_legacy) {
            // 故障注入: 用修复前口径装载 ⇒ [G1] 必须判红（本门判别力自证）
            const std::string content = slurp(transcribed);
            st = legacy_text_search_control(content, "Baader R", wl, tr)
                     ? cj::LoadStatus::kOk
                     : cj::LoadStatus::kCurveNotFound;
        } else {
            st = cj::load_curve(transcribed, "Baader R", &wl, &tr);
        }
        std::printf("  [G1] 转录版 + Baader R%s: status=%s curve=%s\n",
                    inject_legacy ? " (fault-inject=legacy)" : "",
                    cj::status_name(st), curve_repr(wl).c_str());
        check(st == cj::LoadStatus::kOk && wl.size() == 73 && tr.size() == 73 &&
                  wl.front() == 572.0 && wl.back() == 716.0,
              "[G1] 转录版 eng/packaging/config/filters.json 取到 Baader R (73 点 / 572-716 nm)");
    }
    {
        std::vector<double> wl, tr;
        const cj::LoadStatus st = cj::load_curve(original, "Baader R", &wl, &tr);
        std::printf("  [G2] 原始版 + Baader R: status=%s curve=%s\n",
                    cj::status_name(st), curve_repr(wl).c_str());
        check(st == cj::LoadStatus::kOk && wl.size() == 73 &&
                  wl.front() == 572.0 && wl.back() == 716.0,
              "[G2] 原始版 response_curves/filters.json 取到 Baader R (无回归)");
    }

    // ── 绿: orchestrator 路径（FILTER 关键字 → 库键 → 曲线）────────────────
    {
        const std::string key = cj::map_filter_name("R");
        std::vector<double> wl, tr;
        const cj::LoadStatus st = cj::load_curve(transcribed, key, &wl, &tr);
        std::printf("  [G3] orchestrator 路径 FILTER='R' → '%s': status=%s curve=%s\n",
                    key.c_str(), cj::status_name(st), curve_repr(wl).c_str());
        check(key == "Baader R" && st == cj::LoadStatus::kOk && wl.size() == 73 &&
                  wl.front() == 572.0 && wl.back() == 716.0,
              "[G3] orchestrator 路径 (map_filter_name + load_curve) 取到 Baader R (73 点)");
    }

    // ── 红对照: 修复前口径（本门能红的证据）────────────────────────────────
    {
        const std::string content = slurp(transcribed);
        check(!content.empty(), "[R0] 转录版 filters.json 可读（前置）");
        std::vector<double> wl, tr;
        const bool ok = legacy_text_search_control(content, "Baader R", wl, tr);
        std::printf("  [R1] 修复前口径 + 转录版: ok=%d curve=%s\n", ok ? 1 : 0,
                    curve_repr(wl).c_str());
        check(ok && wl.size() == 53 && wl.front() == 420.0 && wl.back() == 524.0,
              "[R1] 修复前口径在转录版上取到 Antlia V Pro Series B (53 点 / 420-524 nm) ⇒ 该口径判红");
    }
    {
        const std::string content = slurp(original);
        std::vector<double> wl, tr;
        const bool ok = legacy_text_search_control(content, "Baader R", wl, tr);
        std::printf("  [R2] 修复前口径 + 原始版: ok=%d curve=%s\n", ok ? 1 : 0,
                    curve_repr(wl).c_str());
        check(ok && wl.size() == 73 && wl.front() == 572.0,
              "[R2] 修复前口径在原始版上本就正确（缺陷只在转录版触发）");
    }

    // ── 负例: 不存在的曲线名 ───────────────────────────────────────────────
    for (const std::string& path : {transcribed, original}) {
        std::vector<double> wl, tr;
        const cj::LoadStatus st = cj::load_curve(path, "No Such Filter Curve", &wl, &tr);
        std::printf("  [N1] %s + 不存在名: status=%s\n", path.c_str(), cj::status_name(st));
        check(st == cj::LoadStatus::kCurveNotFound && wl.empty(),
              "[N1] 不存在的曲线名 ⇒ 正确拒绝 (不是静默取第一条曲线): " + path);
    }

    // ── 负例: 库内登记的非键反例 "bader r"（lookup.non_key_examples, ERROR）──
    {
        const std::string content = slurp(transcribed);
        std::vector<double> wl, tr;
        const cj::LoadStatus st = cj::load_curve(transcribed, "bader r", &wl, &tr);
        std::printf("  [N2] 转录版 + \"bader r\": status=%s\n", cj::status_name(st));
        check(st == cj::LoadStatus::kCurveNotFound && wl.empty(),
              "[N2] 非库键反例 \"bader r\" ⇒ 正确拒绝 (resolution_rule: ERROR)");
        std::vector<double> lwl, ltr;
        const bool lok = legacy_text_search_control(content, "bader r", lwl, ltr);
        std::printf("  [N2b] 修复前口径 + \"bader r\": ok=%d curve=%s\n", lok ? 1 : 0,
                    curve_repr(lwl).c_str());
        check(lok && lwl.size() == 53,
              "[N2b] 修复前口径把非法名 \"bader r\" 静默解析成第一条曲线 (违反 resolution_rule)");
    }

    // ── 负例: provenance 有名字、filters 段没有（注入夹具）─────────────────
    {
        const std::string content = slurp(fixture);
        check(!content.empty(), "[N3] 注入夹具可读（前置）");
        std::vector<double> wl, tr;
        const cj::LoadStatus st = cj::load_curve(fixture, "Ghost R", &wl, &tr);
        std::printf("  [N3] 夹具 + Ghost R: status=%s\n", cj::status_name(st));
        check(st == cj::LoadStatus::kCurveNameOutsideLibrary && wl.empty(),
              "[N3] provenance 有名字、filters 段没有 ⇒ 显式报错 curve_name_outside_library");
        std::vector<double> lwl, ltr;
        const bool lok = legacy_text_search_control(content, "Ghost R", lwl, ltr);
        std::printf("  [N4] 修复前口径 + 夹具: ok=%d curve=%s\n", lok ? 1 : 0,
                    curve_repr(lwl).c_str());
        check(lok && lwl.size() == 2 && lwl.front() == 420.0,
              "[N4] 非退化对照: 修复前口径在同一夹具上静默取到 filters 段第一条曲线");
    }

    // ══ PASSBAND-IDENTITY-GATE-01: 通带身份门（装配期 fail-closed）═════════
    // 判据（逐条可红）: [I0] 正例（声明通带 ⇒ 绿，且身份自述 = 实际曲线）
    //   [I1] 名字命中但曲线对象自述名不符 / [I2] 点数·波长域不符
    //   [I3] provenance 声明与曲线不符 / [I4] provenance 缺该条目
    //   [I5] 名字检查能过、但曲线不是声明那条（身份门唯一能抓的一类）
    //   [I6] 恒真自检: 正例与全部错误输入的判定**必须不同**
    // 依据: docs/contracts/CONFIG_CONTRACT.md §4（名字解析 = 字节精确、无别名；
    //   provenance 的 curve_stats 由机器门与曲线逐条对账）+ docs/science/
    //   PHOTOMETRY.md §2a.4/§2a.5（通带形状不被零点吸收；比较 sigma_residual
    //   必须声明所用模型通带）+ ENGINEERING_SPEC「每项检查有正例与负例」。
    {
        const std::string real = slurp(transcribed);
        check(!real.empty(), "[I0] 转录版 filters.json 可读（前置）");
        // 注意: "Baader R": { 在文件里出现**两次**（provenance.per_filter 一次、
        // filters 段一次）⇒ 曲线对象必须取**最后一次**出现（rfind）。
        const std::size_t k_baader = real.rfind("\"Baader R\": {");
        const std::size_t k_antlia = real.rfind("\"Antlia V Pro Series B\": {");
        const std::size_t p_baader = real.find("\"Baader R\": {", real.find("\"per_filter\""));
        check(k_baader != std::string::npos && k_antlia != std::string::npos &&
                  p_baader != std::string::npos,
              "[I0] 定位到 Baader R / Antlia V Pro Series B / provenance 三处锚点（前置）");

        // 取 filters 段两条曲线的完整对象文本（括号配平），供替换。
        auto object_at = [&real](std::size_t p) -> std::string {
            const std::size_t q = real.find('{', p);
            if (q == std::string::npos) return std::string();
            int depth = 0;
            for (std::size_t i = q; i < real.size(); ++i) {
                if (real[i] == '{') ++depth;
                else if (real[i] == '}') { if (--depth == 0) return real.substr(q, i - q + 1); }
            }
            return std::string();
        };
        const std::string obj_baader = object_at(k_baader);
        const std::string obj_antlia = object_at(k_antlia);
        check(!obj_baader.empty() && !obj_antlia.empty() &&
                  obj_baader.find("572.0") != std::string::npos &&
                  obj_antlia.find("420.0") != std::string::npos,
              "[I0] 两条曲线对象文本可取且波长域各自正确（前置）");

        // 变异 1: 曲线对象自述名改成别支滤镜（曲线数据仍是 Baader R）。
        std::string m1 = real;
        {
            const std::string from = "\"name\": \"Baader R\"";
            const std::size_t at = m1.find(from, k_baader);
            check(at != std::string::npos, "[I0] 变异 1 锚点存在（前置）");
            m1.replace(at, from.size(), "\"name\": \"Antlia V Pro Series B\"");
        }
        // 变异 2: Baader R 对象**内部**的 value 数组被整段替换成别支滤镜的值
        //   （名字/点数/波长数组不动 ⇒ 名字判据与点数判据都放行；只有
        //   provenance 声明的透过率域对不上 ⇒ 命中 provenance 对账那一项）。
        std::string m2 = real;
        {
            auto value_array_of = [](const std::string& obj) -> std::string {
                const std::size_t k = obj.find("\"value\"");
                if (k == std::string::npos) return std::string();
                const std::size_t b0 = obj.find('[', k);
                const std::size_t b1 = obj.find(']', b0);
                if (b0 == std::string::npos || b1 == std::string::npos) return std::string();
                return obj.substr(b0, b1 - b0 + 1);
            };
            const std::string v_baader = value_array_of(obj_baader);
            // 等长但数值不同（每项 +0.05）⇒ 点数/波长数组/名字全不动，只有
            // provenance 的透过率指纹对不上 ⇒ 命中身份判据的逐元素那一项。
            const std::string v_shifted = retune_number_array(v_baader, 0.05, 3);
            const std::size_t at = m2.find(v_baader, k_baader);
            if (!v_baader.empty() && !v_shifted.empty() && at != std::string::npos &&
                at < k_baader + obj_baader.size()) {
                m2.replace(at, v_baader.size(), v_shifted);
            } else {
                check(false, "[I0] 变异 2 锚点存在（前置）");
            }
        }
        // 变异 3: 曲线数据正确，但 provenance 声明的点数/波长域被改成别支滤镜的。
        std::string m3 = real;
        {
            const std::string from = "\"n_points\": 73,\n          \"wl_min\": 572.0,\n          \"wl_max\": 716.0";
            const std::size_t at = m3.find(from, p_baader);
            if (at != std::string::npos && at < m3.find("\"filters\": {")) {
                m3.replace(at, from.size(),
                           "\"n_points\": 53,\n          \"wl_min\": 420.0,\n          \"wl_max\": 524.0");
            } else {
                check(false, "[I0] 变异 3 锚点存在（前置）");
            }
        }
        // 变异 4: provenance 里把曲线数据的声明与**另一支滤镜**对调 —— Baader R 条目
        //   改挂 Antlia B 的 curve_stats，Antlia B 条目改挂 Baader R 的（两处名字、
        //   点数、波长域、透过率域全不符）。名字判据在此**通过**（对象自述名仍是
        //   Baader R），只有 provenance 对账能判红。
        std::string m4 = real;
        {
            // provenance 段内按**各自的名字键**定位（per_filter 内键序是字母序，
            // Antlia V Pro Series B 在 Baader R **之前**，不能共用一个起点）。
            // 起点用 provenance 段头（p_baader 指向的是 Baader R 自己的条目，位置
            // 在 Antlia B 之后，不能当作两处的共同起点）。
            const std::size_t p_prov = m4.find("\"per_filter\"");
            const std::size_t p_antlia_prov =
                (p_prov == std::string::npos)
                    ? std::string::npos
                    : m4.find("\"Antlia V Pro Series B\": {", p_prov);
            const std::size_t p_baader_prov =
                (p_prov == std::string::npos)
                    ? std::string::npos
                    : m4.find("\"Baader R\": {", p_prov);
            // 必须在 provenance 段内定位：filters 段的曲线对象里也有同样形状的
            // n_points/wl_min/... 字段（缩进不同，但搜索窗口一旦跨段就会取错）。
            const std::size_t p_filters_key = m4.find("\"filters\": {");
            auto find_in_prov = [&m4, p_prov, p_filters_key](const std::string& needle,
                                                             std::size_t from) -> std::size_t {
                if (p_prov == std::string::npos || from == std::string::npos) return std::string::npos;
                const std::size_t at = m4.find(needle, from);
                if (at == std::string::npos) return std::string::npos;
                if (p_filters_key != std::string::npos && at > p_filters_key)
                    return std::string::npos;
                return at;
            };
            const std::size_t q0 =
                find_in_prov("\"n_points\": 73,\n          \"wl_min\": 572.0,"
                             "\n          \"wl_max\": 716.0,\n          \"val_min\": 0.008,"
                             "\n          \"val_max\": 0.988",
                             p_baader_prov);
            const std::size_t q1 =
                find_in_prov("\"n_points\": 53,\n          \"wl_min\": 420.0,"
                             "\n          \"wl_max\": 524.0,\n          \"val_min\": 0.002,"
                             "\n          \"val_max\": 0.978",
                             p_antlia_prov);
            const std::string blk0 = "\"n_points\": 73,\n          \"wl_min\": 572.0,"
                                     "\n          \"wl_max\": 716.0,\n          \"val_min\": 0.008,"
                                     "\n          \"val_max\": 0.988";
            const std::string blk1 = "\"n_points\": 53,\n          \"wl_min\": 420.0,"
                                     "\n          \"wl_max\": 524.0,\n          \"val_min\": 0.002,"
                                     "\n          \"val_max\": 0.978";
            if (q0 != std::string::npos && q1 != std::string::npos) {
                // 两处**对调**：先替换靠后的那处（两段等长 124 B ⇒ 靠前那处的偏移
                // 不受影响），再替换靠前的那处。
                const std::size_t hi = (q0 > q1) ? q0 : q1;
                const std::size_t lo = (q0 > q1) ? q1 : q0;
                const std::string& blk_hi = (q0 > q1) ? blk0 : blk1;
                const std::string& blk_lo = (q0 > q1) ? blk1 : blk0;
                m4.replace(hi, blk_hi.size(), blk_lo);
                m4.replace(lo, blk_lo.size(), blk_hi);
            } else {
                check(false, "[I0] 变异 4 锚点存在（前置）");
            }
        }
        // 变异 5b: 把 provenance 里 Baader R 的**键**改名 ⇒ 该名字在 per_filter 中
        //   无条目（filters 段曲线对象仍是正确的 Baader R）。
        std::string m5b = real;
        {
            const std::size_t a = m5b.find("\"Baader R\": {", p_baader);
            if (a != std::string::npos && a < m5b.find("\"filters\": {")) {
                m5b.replace(a, std::strlen("\"Baader R\""), "\"Baader R (unregistered)\"");
            } else {
                check(false, "[I0] 变异 5b 锚点存在（前置）");
            }
        }
        // 变异 5: **名字检查能过**的一类 —— filters 段前面插一条重复键
        //   "Baader R"，其 name 字段也写 "Baader R"，但曲线数据是 Antlia B。
        //   名字比对（对象自述名 == 请求名）在此**通过**，只有 provenance 对账
        //   能判红 ⇒ 这是身份门里 provenance 那一项的判别力证据。
        std::string m5 = real;
        {
            std::string dup = obj_antlia;
            const std::string from = "\"name\": \"Antlia V Pro Series B\"";
            const std::size_t at = dup.find(from);
            if (at != std::string::npos)
                dup.replace(at, from.size(), "\"name\": \"Baader R\"");
            m5.insert(k_baader, "\"Baader R\": " + dup + ",\n    ");
        }

        struct IdCase {
            std::string id;
            std::string name;
            std::string path;
            std::string content;
        };
        const std::string mdir = "passband_identity_gate_work";
        const std::string p1 = mdir + "/m1_name_mismatch.json";
        const std::string p2 = mdir + "/m2_curve_swapped.json";
        const std::string p3 = mdir + "/m3_provenance_stats_mismatch.json";
        const std::string p4 = mdir + "/m4_provenance_entry_missing.json";
        const std::string p5 = mdir + "/m5_duplicate_key_wrong_curve.json";
        const std::string p6 = mdir + "/m6_provenance_entry_missing.json";
        const std::vector<IdCase> cases = {
            {"I0", "正例（声明通带 = Baader R）", transcribed, std::string()},
            {"I1", "曲线对象自述名不符（变异 1）", p1, m1},
            {"I2", "value 数组数值被改（变异 2：名字/点数/波长域/包络全不动）", p2, m2},
            {"I3", "provenance 声明的点数/波长域不符（变异 3）", p3, m3},
            {"I4", "provenance 声明与另一支滤镜对调（变异 4：名字检查放行）", p4, m4},
            {"I5", "filters 段重复键 + 错曲线（变异 5：名字检查放行）", p5, m5},
            {"I6", "provenance 缺该条目（变异 5b：provenance 键改名）", p6, m5b},
        };

        // 判定函数（正例与全部错误输入**共用同一个**函数 ⇒ 判据非恒真）。
        // object_name 由判定函数**自己**从曲线对象里读出来（不是回抄请求名），
        // 故正例也带实际身份，可与请求名对照。
        auto verdict_of = [](const std::string& path) -> std::string {
            std::vector<double> wl, tr;
            cj::IdentityMismatch mis;
            const cj::LoadStatus st = cj::load_curve(path, "Baader R", &wl, &tr, &mis);
            std::string v = std::string("status=") + cj::status_name(st);
            if (st == cj::LoadStatus::kOk) {
                // 曲线对象自述名（身份门要求 == 请求名）
                std::string obj_name = "<unreadable>";
                std::string content;
                if (aio_file::read_all(path.c_str(), &content)) {
                    std::string obj;
                    if (cj::find_curve_object(content, "Baader R", &obj))
                        cj::object_string_field(obj, "name", &obj_name);
                }
                v += " curve=" + curve_repr(wl) + " object_name='" + obj_name + "'";
            } else {
                v += " reason=" + (mis.reason.empty() ? std::string("<none>") : mis.reason);
                v += " object_name='" + (mis.object_name_present ? mis.object_name
                                                                 : std::string("<absent>")) + "'";
                v += " n_wl=" + std::to_string(wl.size());
            }
            return v;
        };

        std::vector<std::string> verdicts;
        std::vector<std::string> legacy_verdicts;
        for (const IdCase& c : cases) {
            if (!c.content.empty()) {
                check(write_file(c.path, c.content), "[" + c.id + "] 变异文件可写（前置）");
            }
            const std::string v = verdict_of(c.path);
            verdicts.push_back(v);
            // 对照列（**仅诊断, 非生产代码**）: 修复前口径在同一变异体上的结论。
            // 它证明身份门不是"换个写法的同一个检查"。
            std::vector<double> lwl, ltr;
            const std::string lpath = c.content.empty() ? transcribed : c.path;
            const std::string lcontent = slurp(lpath);
            const bool lok = legacy_text_search_control(lcontent, "Baader R", lwl, ltr);
            const std::string lv = std::string("legacy: ok=") + (lok ? "1" : "0") +
                                   " curve=" + curve_repr(lwl);
            legacy_verdicts.push_back(lv);
            std::printf("  [%s] %s:\n      %s\n      %s\n", c.id.c_str(), c.name.c_str(),
                        v.c_str(), lv.c_str());
            if (c.id == "I0") {
                check(v.find("status=ok") != std::string::npos &&
                          v.find("n=73 [572.0,716.0]") != std::string::npos &&
                          v.find("object_name='Baader R'") != std::string::npos,
                      "[I0] 正例: 声明通带 Baader R ⇒ 装载成功, 曲线 = 73 点 / [572,716] nm,"
                      " 且曲线对象自述名 == 请求名");
            } else {
                check(v.find("status=curve_identity_mismatch") != std::string::npos,
                      "[" + c.id + "] " + c.name + " ⇒ 具名判红 curve_identity_mismatch");
            }
        }
        // 对照证据: 修复前口径在 I1/I5 上**放行**（与身份门结论相反）⇒ 身份门抓到了
        // 名字/结构检查抓不到的一类；在 I2/I3/I4 上与身份门同为拒绝, 但**判词**不同
        // （身份门给具名 reason, 修复前口径给 53 点的错曲线）。
        check(legacy_verdicts[1].find("ok=1") != std::string::npos,
              "[I1] 对照: 修复前口径在「对象自述名不符」上**放行**（身份门判红）");
        check(legacy_verdicts[4].find("ok=1") != std::string::npos,
              "[I4] 对照: 修复前口径在「provenance 声明对调」上**放行**（身份门判红）");
        check(legacy_verdicts[5].find("ok=1") != std::string::npos,
              "[I5] 对照: 修复前口径在「重复键 + 错曲线」上**放行**（身份门判红）");
        // 每条负例的原因码必须落在**预期**的那一项（不是"随便报个错"）。
        // 读法（哪一项判据抓到的）:
        //   I1/I2 名字判据（对象自述名 ≠ 请求名）
        //   I3/I4/I5 provenance 对账（名字判据放行，声明与曲线矛盾）
        //   I6        provenance 对账（该名字在 per_filter 中无条目）
        const char* expect_reason[] = {"", "curve_object_name_differs_from_requested",
                                       "provenance_curve_stats_mismatch",
                                       "provenance_curve_stats_mismatch",
                                       "provenance_curve_stats_mismatch",
                                       "provenance_curve_stats_mismatch",
                                       "provenance_entry_missing"};
        for (std::size_t i = 1; i < cases.size(); ++i) {
            const std::string want = std::string("reason=") + expect_reason[i];
            check(verdicts[i].find(want) != std::string::npos,
                  "[" + cases[i].id + "] 原因码必须是 " + expect_reason[i] +
                      "（判据定位到预期的那一项）");
        }
        // [I8] 恒真自检（AGENTS §5）: 判定函数在正例与**每一条**错误输入上必须给出
        //      不同结论。若任一错误输入的判定与正例相同 ⇒ 该输入对判据无影响 ⇒
        //      判据对它是恒真的，不具证据资格。反向也查: 两条不同错误输入若给出
        //      完全相同的判定，登记为提示（不判红）。
        bool discriminating = true;
        for (std::size_t i = 1; i < cases.size(); ++i) {
            if (verdicts[i] == verdicts[0]) {
                std::printf("  [I8] %s 的判定与正例相同 ⇒ 判据对该输入恒真\n",
                            cases[i].id.c_str());
                discriminating = false;
            }
        }
        check(discriminating,
              "[I8] 恒真自检: 正例与全部错误输入的判定互不相同（判据有判别力）");
        // 反向对照: 正例的判定必须**不是**判红，否则本门恒假。
        check(verdicts[0].find("status=ok") != std::string::npos,
              "[I8] 反向对照: 正例不得判红（判据非恒假）");
        // 接线证据: 生产唯一实现的装配路径必须真的带上身份门（源码级断言）。
        const std::string fit_src_txt = slurp(fit_src);
        check(fit_src_txt.find("kCurveIdentityMismatch") != std::string::npos &&
                  fit_src_txt.find("declared_filter_passband") != std::string::npos,
              "[I9] 生产拟合入口接线: 身份门失败具名上报 + 声明通带核对");
        // [I10] 调度器落盘接线锁: 身份自述必须真的进 p1_phot.json 与节点 manifest,
        //   且组内一致性判据（首帧落定 + 后续帧逐项核对）必须在位。删掉任一环 ⇒ 本
        //   用例判红（防止「装配检查做了但产物不可核」）。
        const std::string ad_src = slurp(repo + "/lib/infrastructure/scheduler/src/module_adapters.cpp");
        check(!ad_src.empty(), "[I10] module_adapters.cpp 可读（前置）");
        const char* wiring[] = {
            "{\"passband_identity\", passband_identity_json()}",
            "(*man)[\"passband_identity\"] = passband_identity_json();",
            "freq.declared_filter_passband = doc.value(\"filter_passband\", std::string());",
            "PHOT_PASSBAND_IDENTITY_INCONSISTENT",
        };
        bool wired = true;
        for (const char* w : wiring) {
            if (ad_src.find(w) == std::string::npos) {
                std::printf("  [I10] 缺接线: %s\n", w);
                wired = false;
            }
        }
        check(wired,
              "[I10] 调度器落盘接线: p1_phot.json + manifest 自述 + 声明通带送入口 + 组内一致性判据");
        // [I10] 的判别力自证（AGENTS §5）: 同一判据跑在「接线被删掉」的变异源上必须
        //   判红。否则该接线检查是恒真占位（例如关键词恰好出现在注释里）。
        {
            const std::string marker = "(*man)[\"passband_identity\"] = passband_identity_json();";
            const std::size_t at = ad_src.find(marker);
            if (at != std::string::npos) {
                std::string mutated = ad_src;
                mutated.erase(at, marker.size());
                bool mutated_wired = true;
                for (const char* w : wiring) {
                    if (mutated.find(w) == std::string::npos) mutated_wired = false;
                }
                check(!mutated_wired,
                      "[I10] 判别力自证: 删掉 manifest 自述接线后同一判据判红（非恒真）");
            } else {
                check(false, "[I10] 判别力自证: 变异锚点存在（前置）");
            }
        }
        // 变异体留在 ctest 工作目录（构建产物区，不入库），供人工复核；判定已
        // 记录在 stdout，故不删除（下一轮运行覆盖同名文件）。
    }

    // ── 接线: 两个调用方 TU 只调唯一实现, 不再含修复前解析器 ───────────────
    {
        const std::string orch = slurp(orch_src);
        const std::string fit = slurp(fit_src);
        check(!orch.empty() && !fit.empty(), "[W0] 两个调用方源文件可读（前置）");
        check(orch.find("curve_json::load_curve(") != std::string::npos &&
                  orch.find("curve_json::map_filter_name(") != std::string::npos,
              "[W1] orchestrator.cpp 调用唯一实现 (curve_json::load_curve / map_filter_name)");
        check(fit.find("curve_json::load_curve(") != std::string::npos &&
                  fit.find("curve_json::map_filter_name(") != std::string::npos,
              "[W2] frame_photometry_fit.cpp 调用唯一实现");
        const char* legacy_markers[] = {
            "content.find(arr_key, pos)",       // 修复前解析器的数组定位
            "size_t pos = content.find(key);",  // 修复前解析器的键定位
            "load_filter_curve(",               // 修复前的第二份实现
            "load_qe_curve(",
        };
        bool clean = true;
        for (const char* mk : legacy_markers) {
            if (orch.find(mk) != std::string::npos || fit.find(mk) != std::string::npos) {
                std::printf("  [W3] 残留修复前解析器标记: %s\n", mk);
                clean = false;
            }
        }
        check(clean, "[W3] 两个 TU 内无修复前解析器残留（死副本清零）");
    }

    if (inject_legacy) {
        // 注入语义: rc=0 = 注入被检出（[G1] 判红）⇒ 本门有判别力; rc=1 = 未检出。
        const bool detected = (g_fail > 0);
        std::printf("PHOTOCURVE-ORCH-01 FAULT_INJECT(legacy) %s (fail=%d)\n",
                    detected ? "DETECTED" : "NOT_DETECTED", g_fail);
        return detected ? 0 : 1;
    }
    std::printf("PHOTOCURVE-ORCH-01 %s (fail=%d)\n",
                g_fail == 0 ? "ALL_PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
