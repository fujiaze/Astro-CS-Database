// eng/tests/unit/photo_identity_test.cpp — 施加标度身份（Issue #5 A）独立单元测试。
//
// 被测生产 API（lib/include/acsd/normalize_precision.h, 实现于 scheduler/src/
// normalize_precision.cpp）:
//   canonical_double / photo_application_identity / photo_response_identity
//
// 判据（每条都打印实测读数, 失败不静默）:
//   1. canonical_double **无损**: 任意 binary64 → hexfloat 文本 → strtod 逐位回读
//      （含 1.524215866973e-17、其末位邻居、±0、denormal、极大/极小、inf/nan 具名）。
//   2. canonical_double 把不同 binary64 映射到不同串（单射, 有限域内实证）。
//   3. canonical_double **locale 无关**: 在逗号小数分隔符 locale 下输出不变
//      （locale 未安装则如实 SKIP, 不谎报 PASS）。
//   4. photo_application_identity: 同输入同 ID; 极小 k 末位变化 → 不同 ID
//      （**对照** std::to_string 给出同一 "0.000000" 的前置缺陷）;
//      空间系数/中心/归一/order/status/identifiable、输入 occurrence 内容、
//      滤镜/QE 内容摘要任一变化 → 不同 ID; 格式 = photoapplied:v3:<32 hex>
//      （v3 = 逐帧**有序内容**摘要 + 物理响应 id; 见 normalize_precision.h）。
//   5. photo_response_identity: 同键同内容同 ID; 同键不同内容 → 不同 ID;
//      内容摘要缺失 → @none（不写假摘要）; 双空键 → 空串（不虚构身份）。
//   6. writer/reader 往返: FrameScienceDocument.photometry 的 application_id /
//      response_id 经生产序列化→反序列化后逐字不变。
#include <unistd.h>   // getpid

#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <locale>
#include <string>
#include <vector>

#include "acsd/normalize_precision.h"

using acsd::phase1::precision::AppliedScaleIdentity;
using acsd::phase1::precision::canonical_double;
using acsd::phase1::precision::photo_application_identity;
using acsd::phase1::precision::photo_response_identity;

static int g_fail = 0;
static void chk(bool ok, const std::string& what, const std::string& detail = "") {
  std::printf("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", what.c_str(),
              detail.empty() ? "" : " — ", detail.c_str());
  if (!ok) ++g_fail;
}

static std::string hexbits(double v) {
  std::uint64_t b = 0;
  std::memcpy(&b, &v, sizeof(b));
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)b);
  return std::string(buf);
}

int main() {
  std::printf("== 施加标度身份单元测试 ==\n");

  // ── 1/2. canonical_double 无损 + 单射 ───────────────────────────────────
  {
    const std::vector<double> vals = {
        1.524215866973e-17, 1.524315866973e-17, 1.5242158669730001e-17,
        0.0, -0.0, 1.0, -1.0, 1e-300, 5e-324, 1.7976931348623157e308,
        0.1, 1.0 / 3.0, 2.2250738585072014e-308};   // 含最小 denormal 与 0.1
    std::vector<std::string> texts;
    bool inj = true, rt = true;
    for (double v : vals) {
      const std::string t = canonical_double(v);
      texts.push_back(t);
      // 无损: strtod 逐位回读。std::to_chars(hex) 按标准**不带** "0x" 前缀
      // （等价 printf %a 但去前缀）, 故回读时按 C99 形补回（符号在 0x 之前）。
      std::string parse = t;
      const std::size_t ins = (parse[0] == '-') ? 1 : 0;
      parse.insert(ins, "0x");
      char* end = nullptr;
      const double back = std::strtod(parse.c_str(), &end);
      if (end == nullptr || *end != '\0' || hexbits(back) != hexbits(v)) {
        rt = false;
        std::printf("    roundtrip mismatch v=%s text=%s back=%s\n",
                    hexbits(v).c_str(), t.c_str(), hexbits(back).c_str());
      }
    }
    for (std::size_t i = 0; i < texts.size(); ++i)
      for (std::size_t j = i + 1; j < texts.size(); ++j)
        if (hexbits(vals[i]) != hexbits(vals[j]) && texts[i] == texts[j]) inj = false;
    std::printf("    sample: canonical_double(1.524215866973e-17)=%s\n",
                canonical_double(1.524215866973e-17).c_str());
    chk(rt, "canonical_double lossless (hexfloat -> strtod bit-identical, incl. denormal/-0)",
        std::to_string(vals.size()) + " values");
    chk(inj, "canonical_double injective on distinct binary64 values");
    chk(canonical_double(1.524215866973e-17) != canonical_double(1.524315866973e-17),
        "two adjacent ~1e-17 scales serialize differently (the #5 defect)");
  }

  // ── 3. locale 无关 ──────────────────────────────────────────────────────
  {
    const double v = 1.524215866973e-17;
    const std::string before = canonical_double(v);
    // 自定义 numpunct（',' 小数分隔符）: 不依赖系统 locale 文件, 一定可构造 ⇒
    // 该判据总是真的被执行, 不用 SKIP 掩盖。C locale 也一并切换验证。
    struct CommaPunct : std::numpunct<char> {
      char do_decimal_point() const override { return ','; }
    };
    const std::locale old_cpp = std::locale::global(
        std::locale(std::locale::classic(), new CommaPunct));
    const char* old_c = std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
    const char* used = old_c ? "de_DE.UTF-8 (installed)" : "unavailable";
    const std::string after = canonical_double(v);
    std::locale::global(old_cpp);
    std::setlocale(LC_NUMERIC, "C");
    chk(after == before && after.find(',') == std::string::npos,
        "canonical_double locale-independent (unchanged under comma numpunct + LC_NUMERIC)",
        std::string("LC_NUMERIC=") + used + " -> " + after);
  }

  // ── 4. photo_application_identity ───────────────────────────────────────
  {
    AppliedScaleIdentity base;
    base.frame_key = "M42_M4_T2_flying_dutchman-20251212_033644-600S-H-alpha";
    base.input_source_sha256 = std::string(64, 'a');
    base.k_photo = 1.524215866973e-17;
    base.spatial_order = 2;
    base.spatial_status = "ok";
    base.spatial_identifiable = true;
    base.spatial_coef = {0.0123, -0.0456, 1e-6, -2e-7, 3e-8};
    base.spatial_center = {0.1, 0.2, 0.3, 0.4, 0.5};
    base.spatial_norm = {2048.0, 1024.0, 2048.0, 1024.0};
    base.filter_key = "Baader 7nm H-alpha";
    base.filter_content_sha256 = std::string(64, 'b');
    base.qe_key = "KAF-16803";
    base.qe_content_sha256 = std::string(64, 'c');

    const std::string id = photo_application_identity(base);
    std::printf("    application_id(%s) = %s\n", base.frame_key.substr(0, 20).c_str(),
                id.c_str());
    chk(id == photo_application_identity(base), "identical inputs -> identical ID");
    chk(id.rfind("photoapplied:v3:", 0) == 0 && id.size() == std::strlen("photoapplied:v3:") + 32,
        "ID format = photoapplied:v3:<sha256[0:32]>");
    // 索引: 每个参与量单独变化都必须换 ID
    struct Mut { const char* name; void (*f)(AppliedScaleIdentity&); };
    const Mut muts[] = {
        {"k 末位 +1e-26 (1.524315866973e-17)", [](AppliedScaleIdentity& x) { x.k_photo = 1.524315866973e-17; }},
        {"k -> k*(1+2^-52) (1 ulp)", [](AppliedScaleIdentity& x) { x.k_photo = std::nextafter(x.k_photo, 1.0); }},
        {"spatial_coef[0] 末位", [](AppliedScaleIdentity& x) { x.spatial_coef[0] = std::nextafter(x.spatial_coef[0], 1.0); }},
        {"spatial_center[4] 末位", [](AppliedScaleIdentity& x) { x.spatial_center[4] = std::nextafter(x.spatial_center[4], 1.0); }},
        {"spatial_norm[1] 末位", [](AppliedScaleIdentity& x) { x.spatial_norm[1] = std::nextafter(x.spatial_norm[1], 1.0); }},
        {"spatial_order 2->1", [](AppliedScaleIdentity& x) { x.spatial_order = 1; }},
        {"spatial_status ok->degraded", [](AppliedScaleIdentity& x) { x.spatial_status = "degraded"; }},
        {"spatial_identifiable true->false", [](AppliedScaleIdentity& x) { x.spatial_identifiable = false; }},
        {"input occurrence sha 变化", [](AppliedScaleIdentity& x) { x.input_source_sha256 = std::string(64, 'd'); }},
        {"filter content sha 变化", [](AppliedScaleIdentity& x) { x.filter_content_sha256 = std::string(64, 'e'); }},
        {"qe content sha 变化", [](AppliedScaleIdentity& x) { x.qe_content_sha256 = std::string(64, 'f'); }},
        {"filter_key 变化", [](AppliedScaleIdentity& x) { x.filter_key = "Astrodon E-series R"; }},
        // Issue #106: 逐帧**物理响应** id 参与施加身份（参数化窄带没有滤镜曲线
        // 文件 ⇒ 缺它则两个不同响应共享同一 application_id）。
        {"response_id 变化", [](AppliedScaleIdentity& x) { x.response_id = "response:v1:0123456789abcdef0123456789abcdef"; }},
    };
    int bad = 0;
    for (const Mut& m : muts) {
      AppliedScaleIdentity x = base;
      m.f(x);
      if (photo_application_identity(x) == id) {
        std::printf("    [MISS] %s 未改变 ID\n", m.name);
        ++bad;
      }
    }
    chk(bad == 0, "every participating quantity changes the ID",
        std::to_string(sizeof(muts) / sizeof(muts[0])) + " mutations, " +
            std::to_string(bad) + " missed");

    // 前置缺陷对照（不是 PASS/FAIL 判据, 是"为什么会碰撞"的实测证据）
    const double k1 = 1.524215866973e-17, k2 = 1.524315866973e-17;
    std::printf("    [before-fix contrast] std::to_string(k1)=\"%s\" std::to_string(k2)=\"%s\"\n",
                std::to_string(k1).c_str(), std::to_string(k2).c_str());
    chk(std::to_string(k1) == std::to_string(k2),
        "pre-fix contrast: std::to_string collapses two distinct k to the same text");
  }

  // ── 5. photo_response_identity ──────────────────────────────────────────
  {
    const std::string sha_b(64, 'b'), sha_c(64, 'c');
    const std::string r1 = photo_response_identity("Baader 7nm H-alpha", sha_b,
                                                   "KAF-16803", sha_c);
    std::printf("    response_id = %s\n", r1.c_str());
    chk(r1 == photo_response_identity("Baader 7nm H-alpha", sha_b, "KAF-16803", sha_c),
        "identical curve keys+content -> identical response_id");
    chk(r1 != photo_response_identity("Baader 7nm H-alpha", std::string(64, 'd'),
                                      "KAF-16803", sha_c),
        "same file name, different curve content -> different response_id (#5 A.3)");
    chk(photo_response_identity("F", "", "Q", "").find("@none") != std::string::npos,
        "missing content digest -> explicit @none (no fabricated digest)");
    chk(photo_response_identity("", "", "", "").empty(),
        "no filter and no QE -> empty response_id (no fabricated identity)");
  }

  // ── 6. writer/reader 往返（application_id/response_id 逐字保持）──────────
  {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
                          ("acsd_photo_identity_" + std::to_string(::getpid()));
    fs::create_directories(root);
    acsd::normalize::FrameScienceDocument doc;
    doc.run_id = "run-identity-test";
    doc.frame_id = "frame-identity-test";
    // 读回方对 solved 帧做物理一致性核对（wcs.cd 非奇异等）, 用真实量级的 CD
    doc.wcs.crval_deg = {84.23144817784576, -6.368125948355615};
    doc.wcs.crpix = {2048.5, 2048.5};
    doc.wcs.cd_deg_per_pixel = {4.811436946842463e-06, 0.0002685204007420565,
                                -0.00026853801154566885, 4.8237842820453135e-06};
    doc.photometry.status = "relative_only";
    doc.photometry.application_id = photo_application_identity(AppliedScaleIdentity{
        "frame-identity-test", std::string(64, 'a'), 1.524215866973e-17, 2, "ok", true,
        {0.1, 0.2, 0.3, 0.4, 0.5}, {0.0, 0.0, 0.0, 0.0, 0.0},
        {2048.0, 1024.0, 2048.0, 1024.0}, "Baader 7nm H-alpha", std::string(64, 'b'),
        "KAF-16803", std::string(64, 'c')});
    doc.photometry.response_id = photo_response_identity(
        "Baader 7nm H-alpha", std::string(64, 'b'), "KAF-16803", std::string(64, 'c'));
    acsd::normalize::ModelWriteResult wr;
    const bool wrote = acsd::normalize::write_frame_science_document(root.string(), doc, &wr);
    chk(wrote && wr.ok, "writer accepts the identity fields", wr.error);
    if (wrote && wr.ok) {
      const acsd::normalize::FrameScienceReadResult rr =
          acsd::normalize::read_frame_science_document(
              (root / "frame_science.json").string());
      chk(rr.ok && rr.doc.photometry.application_id == doc.photometry.application_id &&
              rr.doc.photometry.response_id == doc.photometry.response_id,
          "writer/reader roundtrip keeps application_id/response_id verbatim",
          rr.ok ? rr.doc.photometry.application_id : rr.error);
    }
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  std::printf("\n%s: %d failure(s)\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
  return g_fail ? 1 : 0;
}
