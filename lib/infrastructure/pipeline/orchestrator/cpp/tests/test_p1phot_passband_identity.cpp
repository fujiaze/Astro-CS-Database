// ============================================================================
// test_p1phot_passband_identity.cpp - 测光装配期**通带身份门**（生产拟合入口）
//                                      PASSBAND-IDENTITY-GATE-01
// ----------------------------------------------------------------------------
// 被测面 = 生产装配入口 astrocs::photometry::fit_frame_photometry
//   （scheduler/module_adapters.cpp 的 photometry 节点 → 本入口 → 冻结 C 入口）。
// 规范依据:
//   - docs/science/PHOTOMETRY.md §2a.4「比较不同帧/不同模型的 sigma_residual 时
//     必须声明所用模型通带」+ §2a.5「通带形状不被零点吸收」（合成 F_syn 用的
//     通带不是配置声明的那条 ⇒ 结果不可比, 必须拒绝）;
//   - docs/contracts/CONFIG_CONTRACT.md §4（滤镜名解析 = 字节精确、无别名；
//     provenance 的 curve_stats 与曲线逐条对账）;
//   - eng/packaging/config/filters.json#lookup.resolution_rule
//     「resolve(name) = filters[name] if name in keys(filters) else ERROR」。
// 判据（逐条可红）:
//   [F1] 红: 声明通带 'Antlia V Pro Series B' 而 FILTER='Baader R' ⇒ 具名拒绝,
//        且判词含两边的名字（passband identity mismatch）
//   [F2] 绿对照: 声明通带与 FILTER 一致（都解析到同一库键）⇒ 不得因身份判据拒绝
//        （身份判据在装载阶段通过; 后续失败必须是别的原因, 如星表缺失）
//   [F3] 红: 声明通带名**不是库键**（'Baader  R' 双空格）⇒ 具名拒绝（无归一化）
//   [F4] 恒真自检: [F1]/[F3] 的判词必须不同（同一判定函数对两个错误输入给出不同
//        结论）; 且 [F2] 的判词与两者都不同 ⇒ 判据有判别力, 不是恒真/恒假
//   [F5] 非空断言: 每条拒绝都必须带 error + failure_scope=kEnvironment
//        （不得返回 fit_ok=true 或空判词）
// 本 TU 只覆盖**装载阶段之前/之中**的判据（不需要星表数据 ⇒ 无 Gaia 依赖）;
// 身份自述字段（filter_key/n_points/波长域）由生产入口在同一路径上填充, 其
// 落盘与一致性判据由 orchestrator_curve_resolve_gate 的 [I0..I9] 覆盖。
// ============================================================================

#include "frame_photometry_fit.h"

#include "aio_file_io.h"        // aio_file::read_all / RandomWriter（aio 唯一 I/O 实现）
#include "aio_atomic_file.h"    // aio_atomic::make_dirs（文件系统原语）

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", what.c_str());
    if (!ok) ++g_fail;
}

// 组装一个最小但**合法**的拟合请求：所有"缺项即环境失败"的字段都填好，
// 使失败点必然落在**通带身份判据**上（而不是缺 gaia_dir / 缺 PSF 数组）。
astrocs::photometry::FramePhotFitRequest make_request(const std::string& filters_json,
                                                     const std::string& declared) {
    astrocs::photometry::FramePhotFitRequest req;
    static std::vector<double> px(16 * 16, 1.0);
    static std::vector<double> cx{4.0, 8.0, 12.0};
    static std::vector<double> cy{4.0, 8.0, 12.0};
    static std::vector<double> fl{1000.0, 1200.0, 900.0};
    static std::vector<int> st{0, 0, 0};
    req.pixels = px.data();
    req.width = 16;
    req.height = 16;
    req.psf_cx = cx.data();
    req.psf_cy = cy.data();
    req.psf_flux = fl.data();
    req.psf_status = st.data();
    req.n_psf = 3;
    req.crval1 = 83.2834;
    req.crval2 = -6.3743;
    req.crpix1 = 8.0;
    req.crpix2 = 8.0;
    req.cd11 = -0.0002689;
    req.cd12 = 0.0;
    req.cd21 = 0.0;
    req.cd22 = 0.0002689;
    req.gaia_data_dir = "/nonexistent/gaia-dir-for-identity-gate";
    req.filter_name = "Baader R";
    req.declared_filter_passband = declared;
    req.filters_json = filters_json;
    return req;
}

// 取某个曲线对象（"<key>": {...} 的括号配平文本）。缺失 ⇒ 空串。
std::string curve_object_of(const std::string& content, const std::string& key) {
    const std::string needle = "\"" + key + "\": {";
    const std::size_t p = content.rfind(needle);   // 曲线定义在 filters 段（最后一次出现）
    if (p == std::string::npos) return std::string();
    const std::size_t q = content.find('{', p);
    if (q == std::string::npos) return std::string();
    int depth = 0;
    for (std::size_t i = q; i < content.size(); ++i) {
        if (content[i] == '{') ++depth;
        else if (content[i] == '}') { if (--depth == 0) return content.substr(q, i - q + 1); }
    }
    return std::string();
}

// 写变异体（经 aio 唯一 I/O 边界）。
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

}  // namespace

int main(int argc, char** argv) {
    const std::string repo = (argc > 1) ? argv[1] : ".";
    const std::string filters = repo + "/eng/packaging/config/filters.json";
    std::printf("PASSBAND-IDENTITY-GATE-01 装配期通带身份门 (repo=%s)\n", repo.c_str());

    {
        std::string probe;
        check(aio_file::read_all(filters.c_str(), &probe) && !probe.empty(),
              "[F0] 滤镜库可读（前置）: " + filters);
    }

    // ── [F1] 红: 声明通带与 FILTER 解析结果不一致 ────────────────────────────
    std::string v1, v2, v3;
    {
        const auto req = make_request(filters, "Antlia V Pro Series B");
        const auto res = astrocs::photometry::fit_frame_photometry(req);
        v1 = res.error;
        std::printf("  [F1] declared='Antlia V Pro Series B' FILTER='Baader R': rc=%d fit_ok=%d\n"
                    "       error=%s\n", res.rc, res.fit_ok ? 1 : 0, res.error.c_str());
        check(!res.fit_ok && res.rc != 0 &&
                  res.failure_scope == astrocs::photometry::FitFailureScope::kEnvironment,
              "[F1] 不一致 ⇒ 拒绝产出标度且作用域 = 环境（配置声明矛盾）");
        check(res.error.find("passband identity mismatch") != std::string::npos &&
                  res.error.find("Antlia V Pro Series B") != std::string::npos &&
                  res.error.find("Baader R") != std::string::npos,
              "[F1] 判词具名: 同时指出声明值与解析出的库键");
    }

    // ── [F2] 绿对照: 声明与 FILTER 一致 ⇒ 不得因**身份判据**拒绝 ─────────────
    {
        const auto req = make_request(filters, "Baader R");
        const auto res = astrocs::photometry::fit_frame_photometry(req);
        v2 = res.error;
        std::printf("  [F2] declared='Baader R' FILTER='Baader R': rc=%d fit_ok=%d\n"
                    "       error=%s\n", res.rc, res.fit_ok ? 1 : 0, res.error.c_str());
        check(res.error.find("passband identity mismatch") == std::string::npos &&
                  res.error.find("filter curve load failed") == std::string::npos,
              "[F2] 声明一致 ⇒ 不得因通带身份判据拒绝（身份门在该路径上通过）");
        // 该请求的星表目录不存在 ⇒ 预期在**星表**处失败（身份门已放行）。
        check(!res.error.empty(),
              "[F2] 后续失败必须如实给出判词（不得静默返回空 error）");
    }

    // ── [F3] 红: 声明名不是库键（不做归一化）─────────────────────────────────
    {
        const auto req = make_request(filters, "Baader  R");   // 双空格
        const auto res = astrocs::photometry::fit_frame_photometry(req);
        v3 = res.error;
        std::printf("  [F3] declared='Baader  R'（双空格, 非库键）: rc=%d fit_ok=%d\n"
                    "       error=%s\n", res.rc, res.fit_ok ? 1 : 0, res.error.c_str());
        check(!res.fit_ok && res.error.find("passband identity mismatch") != std::string::npos,
              "[F3] 非库键的声明名 ⇒ 具名拒绝（resolution_rule: 不折叠空白/不做归一化）");
    }

    // ── [F4] 恒真自检: 判定函数对三个输入给出**互不相同**的结论 ──────────────
    {
        std::printf("  [F4] 判定对照: F1≠F2=%d F1≠F3=%d F2≠F3=%d\n",
                    v1 != v2 ? 1 : 0, v1 != v3 ? 1 : 0, v2 != v3 ? 1 : 0);
        check(!v1.empty() && !v2.empty() && !v3.empty() && v1 != v2 && v1 != v3 && v2 != v3,
              "[F4] 恒真自检: 三个输入的判词互不相同（判据有判别力, 非恒真/恒假）");
    }

    // ── [F5] 非空断言: 拒绝路径必须带具名判词与稳定作用域 ────────────────────
    {
        const auto req = make_request(filters, "Antlia V Pro Series B");
        const auto res = astrocs::photometry::fit_frame_photometry(req);
        check(res.rc != 0 && !res.error.empty() &&
                  res.failure_scope == astrocs::photometry::FitFailureScope::kEnvironment &&
                  !res.fit_ok,
              "[F5] 拒绝路径非空: rc!=0 + 非空 error + kEnvironment + fit_ok=false");
    }

    // ── [F6] 红: 注入**真实错通带**（把声明通带的曲线换成 Antlia V Pro Series B）──
    // 这是本门要抓的原始缺陷形态: 配置声明 Baader R, 而解析出的曲线对象是另一支
    // 蓝端滤镜。生产入口必须**具名判红**, 不得静默用错通带合成 F_syn。
    {
        std::string real;
        const bool read_ok = aio_file::read_all(filters.c_str(), &real) && !real.empty();
        check(read_ok, "[F6] 滤镜库可读（前置）");
        const std::string obj_baader = curve_object_of(real, "Baader R");
        const std::string obj_antlia = curve_object_of(real, "Antlia V Pro Series B");
        check(!obj_baader.empty() && !obj_antlia.empty() &&
                  obj_baader.find("572.0") != std::string::npos &&
                  obj_antlia.find("420.0") != std::string::npos,
              "[F6] 两支曲线对象可取且波长域各自正确（前置）");
        // 把 Baader R 的**曲线数据**换成 Antlia B 的（键仍是 "Baader R"）。
        std::string injected = real;
        {
            const std::size_t k = injected.rfind("\"Baader R\": {");
            const std::size_t k_antlia = injected.rfind("\"Antlia V Pro Series B\": {");
            if (k != std::string::npos && k_antlia != std::string::npos) {
                // 只换曲线对象体，键名保留 ⇒ 名字命中仍是 "Baader R"。
                const std::size_t body0 = injected.find('{', k);
                const std::size_t body1 = injected.find('{', k_antlia);
                std::string body_antlia = obj_antlia;
                injected.replace(body0, obj_baader.size(), body_antlia);
            }
        }
        const std::string inj_path = "passband_identity_gate_work/injected_wrong_passband.json";
        check(write_file(inj_path, injected), "[F6] 注入库可写（前置）");
        const auto req = make_request(inj_path, "Baader R");
        const auto res = astrocs::photometry::fit_frame_photometry(req);
        std::printf("  [F6] 注入错通带（声明 Baader R, 曲线换成 Antlia V Pro Series B）:\n"
                    "       rc=%d fit_ok=%d error=%s\n", res.rc, res.fit_ok ? 1 : 0,
                    res.error.c_str());
        check(!res.fit_ok && res.rc != 0 &&
                  res.error.find("curve_identity_mismatch") != std::string::npos,
              "[F6] 注入错通带 ⇒ 生产入口具名判红（curve_identity_mismatch）");
        check(res.error.find("Antlia V Pro Series B") != std::string::npos,
              "[F6] 判词指出**实际取到的**曲线身份（不是只说\"某条曲线\"）");
        check(res.error.find("passband identity mismatch") == std::string::npos ||
                  res.error.find("declared") == std::string::npos,
              "[F6] 该红来自**曲线身份核对**（不是声明名核对那一项）——两项判据互不替代");
    }

    std::printf("PASSBAND-IDENTITY-GATE-01 %s (fail=%d)\n",
                g_fail == 0 ? "ALL_PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
