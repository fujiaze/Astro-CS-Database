// eng/tests/unit/cpu003_profile_v2_test.cpp — CPU-003 (G3) v2 profile 生成与复读单元测试
// 覆盖: v2 schema 字段完整; Oracle 门(错误 kernel 候选被剔除); winner 仅 OK 候选;
//       AVX512 提升<3% 选 AVX2 规则; workers/block 候选派生; verify_profile_v2 正负例。
#include "profile_gen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "acsd/common_abi_v1.h"
#include "bench_harness.h"

extern "C" {
int acsd_host_services_default_v1(acsd_host_services_v1* out, void** state_out);
void acsd_host_services_destroy_state_v1(void* state);
void acsd_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      acsd_host_services_v1* out);
}

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) quick profile 生成: v2 字段完整
  {
    // 合成 build_id 用中性占位版本, 与任何具体发布版本解耦(N3: verify 不钉死版本)
    std::string build_id = "0.0.0-alpha.0+gabcdef123456";
    std::string commit = "abcdef1234567890abcdef1234567890abcdef12";
    std::string cli_sha = std::string(64, 'a');
    auto pb = acsd::backend_host::generate_profile_v2("quick", build_id, commit, cli_sha, "");
    CHECK(!pb.json.empty());
    // schema 字段
    CHECK(pb.json.find("\"schema\": \"acsd.cpu-profile/v2\"") != std::string::npos);
    CHECK(pb.json.find("\"profile_id\": \"sha256:") != std::string::npos);
    CHECK(pb.json.find("\"created_utc\"") != std::string::npos);
    CHECK(pb.json.find("\"memory_bandwidth\"") != std::string::npos);
    CHECK(pb.json.find("\"raw_samples_sha256\"") != std::string::npos);
    CHECK(pb.json.find("\"kernels\"") != std::string::npos);
    // host/build 字段
    CHECK(pb.json.find("\"logical_available\"") != std::string::npos);
    CHECK(pb.json.find("\"quota_signature\"") != std::string::npos);
    CHECK(pb.json.find("\"source_commit\"") != std::string::npos);
    CHECK(pb.json.find("\"benchmark_binary_sha256\"") != std::string::npos);
    // 至少 1 个 kernel, 且每个含规格字段
    CHECK(pb.kernels.size() >= 1);
    for (const auto& [kid, kp] : pb.kernels) {
      CHECK(!kid.empty());
      CHECK(!kp.provider.empty());
      CHECK(kp.workers >= 1);
      CHECK(kp.block >= 1);
      CHECK(kp.median_ns > 0);
      CHECK(kp.mad_ns >= 0);
      CHECK(kp.self_test_sha256.size() == 64);
    }
    // 原始候选非空且 hash 匹配
    CHECK(!pb.raw.empty());
    CHECK(!pb.raw_samples_sha256.empty());
    CHECK(pb.raw_samples_sha256.size() == 64);
  }

  // 2) 复读正例: 生成的 profile 可被独立 verify 通过
  {
    std::string build_id = "0.0.0-alpha.0+gabcdef123456";
    std::string commit = "abcdef1234567890abcdef1234567890abcdef12";
    std::string cli_sha = std::string(64, 'b');
    auto pb = acsd::backend_host::generate_profile_v2("quick", build_id, commit, cli_sha, "");
    const std::string err = acsd::backend_host::verify_profile_v2(pb.json, commit);
    CHECK(err.empty());   // 合法
    // 错误 commit → 拒
    const std::string err2 = acsd::backend_host::verify_profile_v2(pb.json, std::string(40, '0'));
    CHECK(!err2.empty());
    // 篡改 schema → 拒
    const std::string bad = pb.json.find("\"schema\": \"acsd.cpu-profile/v2\"") !=
                            std::string::npos
        ? pb.json.substr(0, pb.json.find("v2") + 2) + "\"x\"" + pb.json.substr(
              pb.json.find("v2") + 3)
        : pb.json;
    const std::string err3 = acsd::backend_host::verify_profile_v2(bad, commit);
    CHECK(!err3.empty());
  }

  // 2b) acsd_version 仅格式校验(N3): 非法形态必须拒; 任意合法版本(含非本构建
  //     字面量)必须过 —— 版本不再钉死, build 绑定由 source_commit 校验承担。
  {
    const std::string build_id = "0.0.0-alpha.0+gabcdef123456";
    const std::string commit = "abcdef1234567890abcdef1234567890abcdef12";
    auto pb = acsd::backend_host::generate_profile_v2("quick", build_id, commit,
                                                         std::string(64, 'c'), "");
    const std::string orig = "\"acsd_version\": \"0.0.0-alpha.0\"";
    const auto pos = pb.json.find(orig);
    CHECK(pos != std::string::npos);
    auto tamper = [&](const std::string& v) {
      const std::string repl = "\"acsd_version\": \"" + v + "\"";
      return pb.json.substr(0, pos) + repl + pb.json.substr(pos + orig.size());
    };
    // 非法形态 → 拒
    CHECK(!acsd::backend_host::verify_profile_v2(tamper("not-a-version"), commit).empty());
    CHECK(!acsd::backend_host::verify_profile_v2(tamper("1.2"), commit).empty());
    CHECK(!acsd::backend_host::verify_profile_v2(tamper(""), commit).empty());
    CHECK(!acsd::backend_host::verify_profile_v2(tamper("1.2.x.3"), commit).empty());
    CHECK(!acsd::backend_host::verify_profile_v2(tamper("1.2.3 bad"), commit).empty());
    // 任意合法 semver(非本构建字面量) → 过 (N3 核心语义)
    CHECK(acsd::backend_host::verify_profile_v2(tamper("99.99.99-alpha.9"), commit).empty());
    CHECK(acsd::backend_host::verify_profile_v2(tamper("0.11.0-alpha.2"), commit).empty());
    CHECK(acsd::backend_host::verify_profile_v2(tamper("0.0.0-alpha.0+gabcdef123456"),
                                                   commit).empty());
  }

  // 3) worker 候选: {1, 中位, 全部} 派生(avail=2 → {1,2})
  {
    auto c2 = acsd::backend_host::worker_candidates(2);
    CHECK(c2.size() == 2 && c2[0] == 1 && c2[1] == 2);
    auto c4 = acsd::backend_host::worker_candidates(4);
    CHECK(c4.size() == 3 && c4[0] == 1 && c4[1] == 2 && c4[2] == 4);
  }

  // 4) AVX512 提升<3% 规则: select_with_noise_margin 选保守者
  {
    std::vector<acsd::backend_host::BenchResult> r;
    r.push_back({"avx512", "OK", "", 7, 100.0, 2.0, 95, 105, 2, "h"});
    r.push_back({"avx2", "OK", "", 7, 98.0, 2.0, 93, 103, 2, "h"});
    // avx2(98) 比 avx512(100) 快 2% < 3% → 但 select_with_noise_margin 的 conservative=avx2
    const std::string w = acsd::backend_host::select_with_noise_margin(r, "avx2", 0.03);
    CHECK(w == "avx2");   // 保守(avx2)胜出
    // 若 avx512 快 5% ≥ 3% → 选 avx512
    std::vector<acsd::backend_host::BenchResult> r2;
    r2.push_back({"avx512", "OK", "", 7, 95.0, 2.0, 90, 100, 2, "h"});
    r2.push_back({"avx2", "OK", "", 7, 100.0, 2.0, 95, 105, 2, "h"});
    const std::string w2 = acsd::backend_host::select_with_noise_margin(r2, "avx2", 0.03);
    CHECK(w2 == "avx512");  // 提升 5% ≥ 3% → 选 AVX512
  }

  // 5) 无 profile 行为: baseline + 有效 worker(available≥2 不退 1)
  {
    auto np = acsd::backend_host::no_profile_policy(2);
    CHECK(np.backend_id == "baseline");
    CHECK(np.workers == 2);
    auto np1 = acsd::backend_host::no_profile_policy(1);
    CHECK(np1.workers == 1);
  }

  // 6) R-52 组装期不变量判据(唯一出处 profile_invariant_violation)的正负例:
  //    正例 = 合规项不得误判; 负例 = "oracle:pass 却 median<=0" 必须判红。
  {
    acsd::backend_host::KernelProfile good;
    good.kernel_id = "hips-bulk-transform";
    good.correctness_test = "oracle:pass";
    good.median_ns = 1234.5;
    good.mad_ns = 12.0;
    CHECK(acsd::backend_host::profile_invariant_violation(good).empty());

    // 负例 1(本轮红项的真形态): 过 oracle 但统计量根本没测到
    acsd::backend_host::KernelProfile zero = good;
    zero.median_ns = 0.0;
    CHECK(acsd::backend_host::profile_invariant_violation(zero) ==
          "kernels.hips-bulk-transform.median <= 0");

    // 负例 2: MAD 为负(非法统计量)
    acsd::backend_host::KernelProfile negmad = good;
    negmad.mad_ns = -1.0;
    CHECK(!acsd::backend_host::profile_invariant_violation(negmad).empty());

    // 不误判: oracle 失败时 median=0 是合法表达(CPU-003: 错误候选不计时) —— 但按 R-53
    // 必须同时带**可判定**证据(有候选执行 + 首次数值不符 ⇒ 代码性), 否则判红(见下)。
    acsd::backend_host::KernelProfile failed = good;
    failed.correctness_test = "oracle:fail";
    failed.median_ns = 0.0;
    failed.evidence.present = true;
    failed.evidence.executed_candidates = 36;
    failed.evidence.tolerance = 2e-4;
    failed.evidence.has_first_mismatch = true;
    failed.evidence.first_mismatch.index = 1;
    failed.evidence.first_mismatch.got = 0.499992;
    failed.evidence.first_mismatch.ref = 0.410044;
    failed.evidence.first_mismatch.size_class = "small";
    failed.evidence.first_mismatch.provider = "baseline";
    failed.evidence.first_mismatch.workers = 1;
    failed.evidence.first_mismatch.block = 512;
    CHECK(acsd::backend_host::profile_invariant_violation(failed).empty());

    // 负例 3(证据缺失 ⇒ 红): 同一失败项拿掉证据即判红 —— 证据缺失不得按环境性放行,
    // 否则"判 FAIL 而不落 mismatch 证据"就成了新的开口子。
    acsd::backend_host::KernelProfile noev = failed;
    noev.evidence = acsd::backend_host::OracleFailEvidence{};
    CHECK(!acsd::backend_host::profile_invariant_violation(noev).empty());
  }

  // 7) Oracle 离散语义正负例(能红能绿): 两档 oracle:fail 的根因是 oracle 把行坐标
  //    写成实值 i/w, 而 kernel 取**整数网格点** i/w(baseline_kernels.h:18「于网格点」/
  //    :27「源采样位置 (x*k, y*k)」; baseline_kernels_impl.inc:150/:212 为整数除)。
  //    本组把该语义钉死: 偏离 ⇒ 判据与冻结公式不同源 ⇒ 所有候选结构性不可能通过(恒红门)。
  {
    const uint32_t w = 8, N = w * w;
    const double tol = 2e-4;   // §7 冻结容差(唯一出处 profile_gen_v2.cpp:55)
    std::vector<float> in1(N, 0.0f), in2(N, 0.0f), in3(N, 0.0f);

    // 7a) wcs-psf-batch: 闭式 = k*exp(-r^2/2), (x,y) = (i%w, i/w) 整数网格点
    {
      const double cx = 3.5, cy = 4.5;
      std::vector<float> in0(N, 0.0f);
      in0[0] = static_cast<float>(cx);
      in0[1] = static_cast<float>(cy);
      const auto ref = acsd::backend_host::oracle_ref_v1(
          ACS_KOP_PSF_BATCH, 1, 1.0f, w, N, in0, in1, in2, in3);
      for (uint32_t i = 0; i < N; ++i) {
        const double x = static_cast<double>(i % w), y = static_cast<double>(i / w);
        const double exact = std::exp(-((x - cx) * (x - cx) + (y - cy) * (y - cy)) * 0.5);
        CHECK(std::fabs(ref[i] - exact) <= tol * std::max(1.0, std::fabs(exact)));
      }
      // 负例(判据能红): 实值行坐标变体在本判据下必须判红(偏离 ≫ 冻结容差);
      // 复现该缺陷即令上面逐点正例与下面这条同时变红。
      const uint32_t probe = w * 3 + 3;   // x=3,y=3 处: 实值 y=3.375 偏离最大
      const double x = static_cast<double>(probe % w);
      const double y_real = static_cast<double>(probe) / static_cast<double>(w);
      const double wrong = std::exp(-((x - cx) * (x - cx) + (y_real - cy) * (y_real - cy)) * 0.5);
      CHECK(std::fabs(wrong - ref[probe]) > tol * std::max(1.0, std::fabs(ref[probe])));
    }

    // 7b) hips-bulk-transform: 闭式 = 双线性(整数行号, 边缘 clamp), k=0.5
    {
      std::vector<float> in0(N, 0.0f);
      for (uint32_t i = 0; i < N; ++i)
        in0[i] = std::sin(static_cast<float>(i) * 0.01f) * 100.0f;   // 与生成路径同构
      in2[0] = static_cast<float>(w);   // 源宽(→ aux0)
      in3[0] = static_cast<float>(w);   // 源高(→ aux1)
      const auto ref = acsd::backend_host::oracle_ref_v1(
          ACS_KOP_HIPS_BULK, 1, 0.5f, w, N, in0, in1, in2, in3);
      for (uint32_t i = 0; i < N; ++i) {
        const double x = static_cast<double>(i % w) * 0.5;
        const double y = static_cast<double>(i / w) * 0.5;
        const int x0 = std::min(std::max(static_cast<int>(std::floor(x)), 0),
                                static_cast<int>(w) - 2);
        const int y0 = std::min(std::max(static_cast<int>(std::floor(y)), 0),
                                static_cast<int>(w) - 2);
        const double fx = x - std::floor(x), fy = y - std::floor(y);
        const double exact = (1 - fx) * (1 - fy) * in0[static_cast<size_t>(y0) * w + x0] +
                             fx * (1 - fy) * in0[static_cast<size_t>(y0) * w + x0 + 1] +
                             (1 - fx) * fy * in0[static_cast<size_t>(y0 + 1) * w + x0] +
                             fx * fy * in0[static_cast<size_t>(y0 + 1) * w + x0 + 1];
        CHECK(std::fabs(ref[i] - exact) <= tol * std::max(1.0, std::fabs(exact)));
      }
      // 负例(判据能红): 本轮实测的第一次偏离点 i=1(small 档实测 got=0.499992/ref=0.410044)
      {
        const double dy = 1.0 / static_cast<double>(w) * 0.5;          // 实值 y 多出的行内偏移
        const double wrong = ref[1] + dy * in0[w];                     // 实值变体在 i=1 的额外项
        CHECK(std::fabs(wrong - ref[1]) > tol * std::max(1.0, std::fabs(ref[1])));
        CHECK(std::fabs(ref[1] - 0.5 * in0[1]) <= tol * std::max(1.0, std::fabs(ref[1])));
      }
    }
  }

  // 8) R-53: oracle 失败性质判别(环境性 vs 代码性)与"无证据 ⇒ 红"的正负例。
  //    动机: 只落 correctness_test="oracle:fail" 时读侧无法区分"本机测不了(环境性)"
  //    与"判据/内核真缺陷(代码性)"; 两档恒 oracle:fail 的真根因正是后者。
  //    判据唯一出处 = profile_gen_v2.cpp:oracle_fail_class / oracle_fail_evidence_violation。
  {
    using acsd::backend_host::KernelProfile;
    using acsd::backend_host::OracleFailClass;
    using acsd::backend_host::OracleFailEvidence;

    // 8a) 正例·环境性: 没有任何候选进入内核执行, 但有**正面**剔除证据
    //     (provider 被加载/ISA/self_test 剔除) ⇒ 本机/本安装树确实测不了。
    {
      OracleFailEvidence env;
      env.present = true;
      env.culled_candidates = 12;
      env.culled_detail.push_back("baseline: self_test_fail");
      env.tolerance = 2e-4;
      CHECK(acsd::backend_host::oracle_fail_class(env) == OracleFailClass::kEnvironmental);
      CHECK(std::string(acsd::backend_host::oracle_fail_class_name(
                acsd::backend_host::oracle_fail_class(env))) == "environmental");
      CHECK(std::string(acsd::backend_host::oracle_fail_kind_name(env)) ==
            "no_candidate_executed");
      CHECK(acsd::backend_host::oracle_fail_evidence_violation("hips-bulk-transform",
                                                                 "oracle:fail", env).empty());
    }

    // 8b) 正例·代码性(数值不符): 有候选进入内核执行且首次不符(索引/got/ref/档/provider 齐)
    {
      OracleFailEvidence code;
      code.present = true;
      code.executed_candidates = 36;
      code.tolerance = 2e-4;
      code.has_first_mismatch = true;
      code.first_mismatch.index = 1;
      code.first_mismatch.got = 0.499992;
      code.first_mismatch.ref = 0.410044;
      code.first_mismatch.size_class = "small";
      code.first_mismatch.provider = "baseline";
      code.first_mismatch.workers = 1;
      code.first_mismatch.block = 512;
      CHECK(acsd::backend_host::oracle_fail_class(code) == OracleFailClass::kCode);
      CHECK(std::string(acsd::backend_host::oracle_fail_kind_name(code)) == "numeric_mismatch");
      CHECK(acsd::backend_host::oracle_fail_evidence_violation("hips-bulk-transform",
                                                                 "oracle:fail", code).empty());
    }

    // 8c) provider 可用却未注册该 kernel = 实现面缺陷 ⇒ 代码性(不得按环境性放行)
    {
      OracleFailEvidence miss;
      miss.present = true;
      miss.culled_candidates = 12;
      miss.missing_kernel_candidates = 12;
      miss.tolerance = 2e-4;
      CHECK(acsd::backend_host::oracle_fail_class(miss) == OracleFailClass::kCode);
      CHECK(std::string(acsd::backend_host::oracle_fail_kind_name(miss)) == "kernel_missing");
      CHECK(acsd::backend_host::oracle_fail_evidence_violation("hips-bulk-transform",
                                                                 "oracle:fail", miss).empty());
    }

    // 8d) 执行了但非数值失败(kernel rc=<非0>) ⇒ 代码性/kernel_error
    {
      OracleFailEvidence rc;
      rc.present = true;
      rc.executed_candidates = 4;
      rc.tolerance = 2e-4;
      rc.detail = "kernel rc=-1";
      CHECK(acsd::backend_host::oracle_fail_class(rc) == OracleFailClass::kCode);
      CHECK(std::string(acsd::backend_host::oracle_fail_kind_name(rc)) == "kernel_error");
    }

    // 8e) 负例·无证据 ⇒ 红(核心): 判 FAIL 而无 mismatch 证据不得按环境性放行
    {
      OracleFailEvidence none;   // present=false 且计数全 0
      CHECK(acsd::backend_host::oracle_fail_class(none) == OracleFailClass::kUndetermined);
      const std::string v = acsd::backend_host::oracle_fail_evidence_violation(
          "wcs-psf-batch", "oracle:fail", none);
      CHECK(!v.empty());
      CHECK(v.find("证据缺失不得按环境性放行") != std::string::npos);
      // 通过且无证据 = 正常(不误判)
      CHECK(acsd::backend_host::oracle_fail_evidence_violation("wcs-psf-batch", "oracle:pass",
                                                                 none).empty());
      // 组装期判据同源: 只写 correctness_test="oracle:fail" 的 kernel 必判红
      KernelProfile kp0;
      kp0.kernel_id = "wcs-psf-batch";
      kp0.correctness_test = "oracle:fail";
      CHECK(!acsd::backend_host::profile_invariant_violation(kp0).empty());
      // 自相矛盾: 声称"没有候选执行"却又报首次不符 ⇒ 不可判定 ⇒ 红
      OracleFailEvidence contra;
      contra.present = true;
      contra.tolerance = 2e-4;
      contra.has_first_mismatch = true;
      contra.first_mismatch.index = 3;
      contra.first_mismatch.size_class = "small";
      contra.first_mismatch.provider = "baseline";
      contra.first_mismatch.workers = 1;
      contra.first_mismatch.block = 512;
      CHECK(acsd::backend_host::oracle_fail_class(contra) == OracleFailClass::kUndetermined);
      CHECK(!acsd::backend_host::oracle_fail_evidence_violation("wcs-psf-batch", "oracle:fail",
                                                                  contra).empty());
    }

    // 8f) 复读层(真 profile 文本上的正负例): 失败可落盘 ⇔ 证据齐; 缺证据/自相矛盾 ⇒ 判红
    {
      const std::string build_id = "0.0.0-alpha.0+gabcdef123456";
      const std::string commit = "abcdef1234567890abcdef1234567890abcdef12";
      auto pb = acsd::backend_host::generate_profile_v2("quick", build_id, commit,
                                                           std::string(64, 'e'), "");
      const std::string key_pass = "\"correctness_test\": \"oracle:pass\",";
      const std::size_t pos = pb.json.find(key_pass);
      CHECK(pos != std::string::npos);
      auto tamper = [&](const std::string& evidence) {
        return pb.json.substr(0, pos) + "\"correctness_test\": \"oracle:fail\"," +
               (evidence.empty()
                    ? std::string()
                    : "\n      \"oracle_fail\": " + evidence + ",") +
               pb.json.substr(pos + key_pass.size());
      };
      const std::string env_ev =
          "{\"class\": \"environmental\", \"kind\": \"no_candidate_executed\", "
          "\"executed_candidates\": 0, \"culled_candidates\": 12, "
          "\"missing_kernel_candidates\": 0, \"tolerance\": 0.0002, "
          "\"culled_detail\": [\"avx512: isa_precheck_failed(avx512f)\"], "
          "\"first_mismatch\": null}";
      // 负例 1: 标成 oracle:fail 却不落证据 ⇒ 复读必须判红(且理由指名证据缺失)
      const std::string e1 = acsd::backend_host::verify_profile_v2(tamper(""), commit);
      CHECK(!e1.empty());
      CHECK(e1.find("oracle_fail missing") != std::string::npos);
      // 正例: 补上环境性证据 ⇒ 复读通过(失败仍可如实落盘, 不退化成写盘失败)
      CHECK(acsd::backend_host::verify_profile_v2(tamper(env_ev), commit).empty());
      // 负例 2: class 声明与计数矛盾(声称环境性却报 36 个执行 + 首次不符) ⇒ 判红
      const std::string lie_ev =
          "{\"class\": \"environmental\", \"kind\": \"no_candidate_executed\", "
          "\"executed_candidates\": 36, \"culled_candidates\": 0, "
          "\"missing_kernel_candidates\": 0, \"tolerance\": 0.0002, "
          "\"first_mismatch\": {\"index\": 1, \"got\": 0.499992, \"ref\": 0.410044, "
          "\"size_class\": \"small\", \"provider\": \"baseline\", \"workers\": 1, "
          "\"block\": 512}}";
      CHECK(!acsd::backend_host::verify_profile_v2(tamper(lie_ev), commit).empty());
      // 负例 3: kind 与证据不符(声称 kernel_error 但报的是首次数值不符) ⇒ 判红
      const std::string badkind_ev =
          "{\"class\": \"code\", \"kind\": \"kernel_error\", "
          "\"executed_candidates\": 36, \"culled_candidates\": 0, "
          "\"missing_kernel_candidates\": 0, \"tolerance\": 0.0002, "
          "\"first_mismatch\": {\"index\": 1, \"got\": 0.499992, \"ref\": 0.410044, "
          "\"size_class\": \"small\", \"provider\": \"baseline\", \"workers\": 1, "
          "\"block\": 512}}";
      CHECK(!acsd::backend_host::verify_profile_v2(tamper(badkind_ev), commit).empty());
      // 负例 4: 容差被抹成 0(证据不完整) ⇒ 判红
      const std::string notol_ev =
          "{\"class\": \"environmental\", \"kind\": \"no_candidate_executed\", "
          "\"executed_candidates\": 0, \"culled_candidates\": 12, "
          "\"missing_kernel_candidates\": 0, \"tolerance\": 0, "
          "\"first_mismatch\": null}";
      CHECK(!acsd::backend_host::verify_profile_v2(tamper(notol_ev), commit).empty());
    }
  }

  if (failures == 0) {
    std::printf("CPU-003 TESTS PASS (v2 profile 字段全/Oracle 门/winner/AVX512<3%%/verify 正负例/组装期不变量正负例/oracle 离散语义正负例/oracle 失败性质证据正负例)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-003 TESTS FAIL (%d)\n", failures);
  return 1;
}
