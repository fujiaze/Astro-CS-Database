// P3-005 单元测试: FITS 原子写 + 重开验证 (dimensions/WCS/BUNIT/标准 DATASUM/CHECKSUM/mask)
// B2-A9: verify 现对 WCS 有鉴别力（CTYPE/CUNIT/CRPIX/CRVAL/CD 对拍），故
// 不再声明"WCS 一致性由写路径单点保证"；本文件另含 WCS 篡改负例。
#include "p3_output.h"
#include "p3_wcs.h"

#include "fitsio.h"   // B2-A9 WCS 篡改负例（独立重开改 header）

#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>
#if defined(_WIN32)
#include <process.h>   // _getpid（P-076 tmp 名确定性断言）
#define P3TEST_GETPID _getpid
#else
#include <unistd.h>    // getpid
#define P3TEST_GETPID getpid
#endif

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 跨平台临时目录 (同 io_adapter_test): Windows 回退 TEMP/TMP/"."。
  const char* d = std::getenv("TMPDIR");
  if (!d || !*d) d = std::getenv("TEMP");
  if (!d || !*d) d = std::getenv("TMP");
#if defined(_WIN32)
  const std::string dir0 = (d && *d) ? std::string(d) : std::string(".");
#else
  const std::string dir0 = (d && *d) ? std::string(d) : std::string("/tmp");
#endif
  // generic_string: 与 p3_assembly_test 同因 — Windows TEMP 反斜杠路径
  // 会被 lib 层 path_is_safe 类校验拒绝(R17 实证), 统一正斜杠。
  const std::string dir = std::filesystem::path(dir0).generic_string();
  const std::string out = dir + "/astrocs_p3_out_test.fits";
  std::remove(out.c_str());

  const int w = 64, h = 48;
  std::vector<float> sig(static_cast<size_t>(w) * h);
  std::vector<float> cov(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      sig[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 100.0f + 0.5f * static_cast<float>(x);
      cov[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = (x < 40) ? 1.0f : 0.0f;  // mask
    }

  astrocs::phase3::P3WcsDescriptor wcs{};
  CHECK(astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.0001389, w, h,
                                     "east_left", 0.0, &wcs) == astrocs::phase3::P3_WCS_OK);

  astrocs::phase3::P3Provenance prov{};
  prov.hips_id = "ivo://astrocs/test";
  prov.manifest_hash = "deadbeef";
  prov.missing_tiles = nullptr;
  prov.missing_count = 0;
  prov.software_version = "0.10.0-alpha.2";   // 来自版本单源, 非硬编码
  prov.run_id = "run-p3-005-test";
  prov.order_sel_used = "4";
  prov.sampler_used = "bilinear";

  astrocs::phase3::P3OutputResult res{};
  // 1) 原子写 (不硬编码 version/run_id: 来自 prov)
  {
    const astrocs::phase3::P3OutputStatus wst = astrocs::phase3::p3_output_write_atomic(
        sig.data(), cov.data(), w, h, &wcs, "ADU", out.c_str(),
        &prov, -32, -1, &res);
    if (wst != astrocs::phase3::P3_OUT_OK)
      std::fprintf(stderr, "[diag] write_atomic status=%d errno=%d\n", (int)wst, errno);
    CHECK(wst == astrocs::phase3::P3_OUT_OK);
  }
  CHECK(res.coverage_ok == 1);
  CHECK(res.reopen_ok == 1);
  CHECK(std::strlen(res.sha256) == 64);

  // 2) 独立重开 verify: dimensions/WCS/BUNIT/checksum/mask 一致
  {
    astrocs::phase3::P3OutputResult v{};
    CHECK(astrocs::phase3::p3_output_verify(out.c_str(), &wcs, sig.data(), cov.data(),
                                            w, h, &v) == astrocs::phase3::P3_OUT_OK);
    CHECK(v.reopen_ok == 1);
    CHECK(v.coverage_ok == 1);
    CHECK(std::strlen(v.sha256) == 64);
  }

  // 2b) FIX-402 (FZ-P3-BUNIT-QUADRATIC / docs/science/DATA_SEMANTICS.md §31.1
  // §1): variance/ivar BUNIT 必须是 signal BUNIT 的**幂次代数**结果（冻结 canonical
  // 串: ADU/sr → ADU^2/sr^2 / sr^2/ADU^2），禁朴素拼接（旧实现写 "ADU/sr^2"）；
  // 不在冻结表内的单位必须显式拒（P3_OUT_PARAM, 不落任何文件）。
  {
    std::vector<float> var(static_cast<size_t>(w) * h, 4.0f);
    std::vector<float> ivar(static_cast<size_t>(w) * h, 0.25f);
    struct BunitCase { const char* sig; const char* want_var; const char* want_ivar; };
    // 第三例 = legacy 读侧兼容（DATA_SEMANTICS §31.1a「读侧兼容旧串 px / pixel，
    // 写侧只出 sr」）: 旧产品 BUNIT="ADU/px^2" 必须可解析（同一立体角维），
    // 且写出的 VARIANCE/IVAR BUNIT 仍是 canonical sr 串。
    const BunitCase bc[3] = {{"ADU", "ADU^2", "ADU^-2"},
                             {"ADU/sr", "ADU^2/sr^2", "sr^2/ADU^2"},
                             {"ADU/px^2", "ADU^2/sr^2", "sr^2/ADU^2"}};
    for (int ci = 0; ci < 3; ++ci) {
      const std::string p2 =
          dir + "/astrocs_p3_out_test_bunit" + std::to_string(ci) + ".fits";
      std::remove(p2.c_str());
      astrocs::phase3::P3OutputResult r2{};
      const astrocs::phase3::P3OutputStatus w2 =
          astrocs::phase3::p3_output_write_atomic_ex(
              sig.data(), cov.data(), var.data(), ivar.data(), w, h, &wcs,
              bc[ci].sig, p2.c_str(), &prov, -32, -1, &r2);
      CHECK(w2 == astrocs::phase3::P3_OUT_OK);
      fitsfile* f = nullptr;
      int st = 0;
      // cfitsio 句柄"取到才可用": fits_* 对 nullptr 句柄直接解引用空指针，进程
      // SEGFAULT 会吞掉真实诊断（CHECK 非致命，失败后仍往下执行）。取句柄处
      // 一律致命化：失败计入 failures 并跳过依赖该句柄的语句。
      if (fits_open_file(&f, p2.c_str(), READONLY, &st) != 0) {
        std::fprintf(stderr, "REQUIRE failed %s:%d: fits_open_file(%s) status=%d\n",
                     __FILE__, __LINE__, p2.c_str(), st);
        ++failures;
        continue;
      }
      char card[FLEN_VALUE];
      st = 0;
      CHECK(fits_movnam_hdu(f, IMAGE_HDU, (char*)"VARIANCE", 0, &st) == 0);
      st = 0;
      CHECK(fits_read_key(f, TSTRING, (char*)"BUNIT", card, nullptr, &st) == 0);
      CHECK(std::strcmp(card, bc[ci].want_var) == 0);
      st = 0;
      CHECK(fits_movnam_hdu(f, IMAGE_HDU, (char*)"IVAR", 0, &st) == 0);
      st = 0;
      CHECK(fits_read_key(f, TSTRING, (char*)"BUNIT", card, nullptr, &st) == 0);
      CHECK(std::strcmp(card, bc[ci].want_ivar) == 0);
      // 数值不变量: ivar == 1/variance（覆盖像素; 同一 C_out = R C_in R^T 面）
      float vbuf = 0.0f, ibuf = 0.0f;
      long fp[2] = {21, 10};
      st = 0;
      CHECK(fits_movnam_hdu(f, IMAGE_HDU, (char*)"VARIANCE", 0, &st) == 0);
      st = 0;
      CHECK(fits_read_pix(f, TFLOAT, fp, 1, nullptr, &vbuf, nullptr, &st) == 0);
      st = 0;
      CHECK(fits_movnam_hdu(f, IMAGE_HDU, (char*)"IVAR", 0, &st) == 0);
      st = 0;
      CHECK(fits_read_pix(f, TFLOAT, fp, 1, nullptr, &ibuf, nullptr, &st) == 0);
      CHECK(std::fabs(ibuf * vbuf - 1.0f) < 1e-5f);
      st = 0;
      CHECK(fits_close_file(f, &st) == 0);
      std::remove(p2.c_str());
    }
    // 不可解析/表外单位 → 显式拒, 且不留产物（含 .tmp 残骸）
    {
      const std::string p3 = dir + "/astrocs_p3_out_test_bunit_bad.fits";
      std::remove(p3.c_str());
      astrocs::phase3::P3OutputResult r3{};
      const astrocs::phase3::P3OutputStatus w3 =
          astrocs::phase3::p3_output_write_atomic_ex(
              sig.data(), cov.data(), var.data(), ivar.data(), w, h, &wcs,
              "Jy/beam", p3.c_str(), &prov, -32, -1, &r3);
      CHECK(w3 == astrocs::phase3::P3_OUT_PARAM);
      CHECK(!std::filesystem::exists(std::filesystem::path(p3)));
      std::error_code ec2;
      for (const auto& entry : std::filesystem::directory_iterator(dir, ec2)) {
        if (ec2) break;
        const std::string fn = entry.path().filename().string();
        if (fn.rfind("astrocs_p3_out_test_bunit_bad", 0) == 0) {
          CHECK(false);   // 拒写不得留下任何残骸（含 .tmp）
          break;
        }
      }
    }
  }

  // 3) 原子性: 无 .tmp 残留
  {
    std::string tmp = dir + "/.astrocs_p3_out_test.";   // 前缀匹配
    bool found = false;
    // 检查目录中是否有 .tmp 残留 (跨平台: filesystem 遍历, WIN-001 替代 popen/ls)
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) break;
      if (entry.path().filename().string().rfind(".astrocs_p3_out_test.", 0) == 0 &&
          entry.path().extension() == ".tmp") { found = true; break; }
    }
    CHECK(!found);
  }

  // 3b) B2-A9 WCS 篡改负例: 独立重开把 CRPIX1 平移 +1（双桥接/原点回归的
  // 典型形态），verify 必须检出（reopen_ok=0）。verify 对 WCS 的鉴别力是
  // 本门的判别力来源，不是同式自证。
  {
    fitsfile* tf = nullptr;
    int st = 0;
    // 同 2b: 取句柄失败即致命化，不把 nullptr 交给 cfitsio。
    if (fits_open_file(&tf, out.c_str(), READWRITE, &st) != 0) {
      std::fprintf(stderr, "REQUIRE failed %s:%d: fits_open_file(%s) status=%d\n",
                   __FILE__, __LINE__, out.c_str(), st);
      ++failures;
    } else {
      double tampered = 0.0;
      CHECK(fits_read_key(tf, TDOUBLE, (char*)"CRPIX1", &tampered, nullptr, &st) == 0);
      tampered += 1.0;   // 0.5px/1px 型系统性平移
      st = 0;
      CHECK(fits_update_key(tf, TDOUBLE, (char*)"CRPIX1", &tampered, nullptr, &st) == 0);
      st = 0;
      CHECK(fits_close_file(tf, &st) == 0);
      astrocs::phase3::P3OutputResult v{ };
      const astrocs::phase3::P3OutputStatus vst =
          astrocs::phase3::p3_output_verify(out.c_str(), &wcs, sig.data(), cov.data(),
                                            w, h, &v);
      CHECK(vst == astrocs::phase3::P3_OUT_OK);   // 文件可读 = OK，但鉴别位必须失败
      CHECK(v.reopen_ok == 0);                    // CRPIX 篡改必须被检出
      // 复原 CRPIX1，保证后续用例共享同一产物文件时仍是合法 WCS。
      tampered -= 1.0;
      st = 0;
      CHECK(fits_open_file(&tf, out.c_str(), READWRITE, &st) == 0);
      CHECK(fits_update_key(tf, TDOUBLE, (char*)"CRPIX1", &tampered, nullptr, &st) == 0);
      st = 0;
      CHECK(fits_close_file(tf, &st) == 0);
      astrocs::phase3::P3OutputResult vr{ };
      CHECK(astrocs::phase3::p3_output_verify(out.c_str(), &wcs, sig.data(), cov.data(),
                                              w, h, &vr) == astrocs::phase3::P3_OUT_OK);
      CHECK(vr.reopen_ok == 1);                   // 复原后鉴别位恢复
    }
  }

  // 3c) P-075 (台账 A1) BUNIT 篡改负例: verify 对 BUNIT 有鉴别力（能红能绿）。
  // 篡改 PRIMARY BUNIT（表外串 / 表内另一 canonical 值）或 VARIANCE BUNIT，
  // verify_ex 必须 reopen_ok=0；复原后恢复 1。（与 3b WCS 负例同模式:
  // 文件可读 ⇒ 状态码 OK，鉴别位置 0；verify 不读 CHECKSUM 键，篡改不另破哈希。）
  {
    const std::string pb = dir + "/astrocs_p3_out_test_bunit_tamper.fits";
    std::remove(pb.c_str());
    std::vector<float> var(static_cast<size_t>(w) * h, 4.0f);
    std::vector<float> ivar(static_cast<size_t>(w) * h, 0.25f);
    astrocs::phase3::P3OutputResult rb{};
    CHECK(astrocs::phase3::p3_output_write_atomic_ex(
              sig.data(), cov.data(), var.data(), ivar.data(), w, h, &wcs,
              "ADU", pb.c_str(), &prov, -32, -1, &rb) ==
          astrocs::phase3::P3_OUT_OK);
    CHECK(rb.reopen_ok == 1);   // 基线绿

    auto verify_bunit = [&](astrocs::phase3::P3OutputResult* v) {
      return astrocs::phase3::p3_output_verify_ex(
          pb.c_str(), &wcs, sig.data(), cov.data(), var.data(), ivar.data(),
          w, h, v);
    };
    // 取句柄失败即致命化（同 2b/3b: nullptr 不得交给 cfitsio）。
    auto set_primary_bunit = [&](const char* val) -> bool {
      fitsfile* tf = nullptr; int st = 0;
      if (fits_open_file(&tf, pb.c_str(), READWRITE, &st) != 0) {
        std::fprintf(stderr, "REQUIRE failed %s:%d: fits_open_file status=%d\n",
                     __FILE__, __LINE__, st);
        ++failures; return false;
      }
      const bool ok = fits_update_key(tf, TSTRING, (char*)"BUNIT", (void*)val,
                                      nullptr, &st) == 0;
      st = 0;
      return ok && fits_close_file(tf, &st) == 0;
    };
    auto set_variance_bunit = [&](const char* val) -> bool {
      fitsfile* tf = nullptr; int st = 0;
      if (fits_open_file(&tf, pb.c_str(), READWRITE, &st) != 0) {
        std::fprintf(stderr, "REQUIRE failed %s:%d: fits_open_file status=%d\n",
                     __FILE__, __LINE__, st);
        ++failures; return false;
      }
      bool ok = fits_movnam_hdu(tf, IMAGE_HDU, (char*)"VARIANCE", 0, &st) == 0;
      st = 0;
      ok = ok && fits_update_key(tf, TSTRING, (char*)"BUNIT", (void*)val,
                                 nullptr, &st) == 0;
      st = 0;
      return ok && fits_close_file(tf, &st) == 0;
    };

    // 负例 A: PRIMARY BUNIT → 表外串（写侧禁发布的单位）⇒ 必红。
    CHECK(set_primary_bunit("Jy/beam"));
    { astrocs::phase3::P3OutputResult v{};
      CHECK(verify_bunit(&v) == astrocs::phase3::P3_OUT_OK);
      CHECK(v.reopen_ok == 0); }   // 表外 BUNIT 必须被检出
    CHECK(set_primary_bunit("ADU"));   // 复原
    { astrocs::phase3::P3OutputResult v{};
      CHECK(verify_bunit(&v) == astrocs::phase3::P3_OUT_OK);
      CHECK(v.reopen_ok == 1); }   // 复原后绿

    // 负例 B: PRIMARY BUNIT → 表内另一 canonical 值（ADU/sr）—— VARIANCE/IVAR
    // 的 BUNIT 随之与二次律推导失配 ⇒ 必红（verify_ex 无期望 BUNIT 入参,
    // 表内值间篡改由二次律交叉对拍检出）。
    CHECK(set_primary_bunit("ADU/sr"));
    { astrocs::phase3::P3OutputResult v{};
      CHECK(verify_bunit(&v) == astrocs::phase3::P3_OUT_OK);
      CHECK(v.reopen_ok == 0); }
    CHECK(set_primary_bunit("ADU"));   // 复原
    { astrocs::phase3::P3OutputResult v{};
      CHECK(verify_bunit(&v) == astrocs::phase3::P3_OUT_OK);
      CHECK(v.reopen_ok == 1); }

    // 负例 C: VARIANCE BUNIT 与 PRIMARY 二次律失配（朴素拼接旧缺陷形态）⇒ 必红。
    CHECK(set_variance_bunit("ADU/sr^2"));
    { astrocs::phase3::P3OutputResult v{};
      CHECK(verify_bunit(&v) == astrocs::phase3::P3_OUT_OK);
      CHECK(v.reopen_ok == 0); }
    CHECK(set_variance_bunit("ADU^2"));   // 复原（"ADU" 的冻结二次律串）
    { astrocs::phase3::P3OutputResult v{};
      CHECK(verify_bunit(&v) == astrocs::phase3::P3_OUT_OK);
      CHECK(v.reopen_ok == 1); }

    std::remove(pb.c_str());
  }

  // 3d) P-077 (台账 A3) 成对发布门负例: begin_hdu/publish 必须校验 HDU 集合
  // 成对完整（只写 PRIMARY 也能发布的旧缺陷形态）。三臂：
  //   (A) 仅 PRIMARY 收尾 → publish 必拒，且无产物、无 .tmp 残留；
  //   (B) PRIMARY+COVERAGE 收尾（unc 不可用面）→ publish 成功（positive control，
  //       证明门不是"恒真拒绝"：门只在该拒的臂上拒）；
  //   (C) begin_hdu(3)=IVAR 在 VARIANCE 未收尾时被拒 + VARIANCE 收尾后未成对
  //       → publish 必拒（单边 uncertainty = 合同违规）。
  {
    const std::string pro077 = dir + "/astrocs_p3_out_test_pr077.fits";
    const std::string pro077b = dir + "/astrocs_p3_out_test_pr077b.fits";
    const std::string pro077c = dir + "/astrocs_p3_out_test_pr077c.fits";
    std::remove(pro077.c_str());
    std::remove(pro077b.c_str());
    std::remove(pro077c.c_str());
    std::error_code ec;
    // 临时对象残骸检出自检：写侧 tmp 名 = <out>.<pid>.tmp（p3_output.cpp:151-157）。
    auto no_residue = [&](const std::string& base) {
      for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) return;
        if (entry.path().filename().string().rfind(base, 0) == 0) { CHECK(false); return; }
      }
    };

    // ---- 臂 A: 只写 PRIMARY ----
    {
      astrocs::phase3::P3FitsStream ws;
      CHECK(ws.open(pro077.c_str(), &wcs, w, h, -32, "ADU", &prov) ==
            astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      astrocs::phase3::P3OutputResult r{};
      CHECK(ws.publish(&r) != astrocs::phase3::P3_OUT_OK);   // 缺 COVERAGE ⇒ 必拒
      CHECK(!ws.published());
      CHECK(!std::filesystem::exists(std::filesystem::path(pro077)));
      ws.abort();
      no_residue("astrocs_p3_out_test_pr077.fits");
    }

    // ---- 臂 B: PRIMARY+COVERAGE（unc 不可用）→ 可发布 ----
    {
      astrocs::phase3::P3FitsStream ws;
      CHECK(ws.open(pro077b.c_str(), &wcs, w, h, -32, "ADU", &prov) ==
            astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(1) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, cov.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      astrocs::phase3::P3OutputResult r{};
      CHECK(ws.publish(&r) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.published());
      CHECK(std::filesystem::exists(std::filesystem::path(pro077b)));
      CHECK(std::strlen(r.sha256) == 64);
      std::remove(pro077b.c_str());
    }

    // ---- 臂 C: VARIANCE 未成对 ----
    {
      astrocs::phase3::P3FitsStream ws;
      CHECK(ws.open(pro077c.c_str(), &wcs, w, h, -32, "ADU", &prov) ==
            astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(1) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, cov.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      // VARIANCE 已收尾、IVAR 缺失（单边 unc）⇒ publish 必拒。
      // 注: 本臂不先试 begin_hdu(3)——序门失败会把流置为 fail-closed 终端态
      // （p3_output.cpp:822-826 impl_->failed=true），后续调用一律 PARAM，
      // 无法再测「VARIANCE 单独收尾」这一面。序门另由臂 C2 单独覆盖。
      CHECK(ws.begin_hdu(2) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      astrocs::phase3::P3OutputResult r{};
      CHECK(ws.publish(&r) != astrocs::phase3::P3_OUT_OK);   // 缺 IVAR ⇒ 必拒
      CHECK(!ws.published());
      CHECK(!std::filesystem::exists(std::filesystem::path(pro077c)));
      ws.abort();
      no_residue("astrocs_p3_out_test_pr077c.fits");
    }

    // ---- 臂 D: HDU 集合成员齐全（不只是"已收尾个数"）----
    // 旧形态只数 hdu_done[0]/[1]，故"跳过 COVERAGE 先建 VARIANCE"这条调用序
    // 会让 hdu_done={1,1} 而发布一个**没有 COVERAGE 扩展**的半成品产品
    // （下游把 support 面缺失误读成"无覆盖"）。三臂：
    //   D1 跳过 COVERAGE 建 VARIANCE → publish 必拒；
    //   D2 COVERAGE 建两次（重复 EXTNAME）→ 第二次 begin_hdu(1) 必拒；
    //   D3 只写 PRIMARY+COVERAGE（unc 不可用）→ 必须仍能发布（正例对照）。
    {
      const std::string p1 = dir + "/astrocs_p3_out_test_pr077e1.fits";
      const std::string p2 = dir + "/astrocs_p3_out_test_pr077e2.fits";
      const std::string p3 = dir + "/astrocs_p3_out_test_pr077e3.fits";
      std::remove(p1.c_str());
      std::remove(p2.c_str());
      std::remove(p3.c_str());
      // D1
      {
        astrocs::phase3::P3FitsStream ws;
        CHECK(ws.open(p1.c_str(), &wcs, w, h, -32, "ADU", &prov) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(2) == astrocs::phase3::P3_OUT_OK);   // 跳过 COVERAGE
        CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
        astrocs::phase3::P3OutputResult r{};
        CHECK(ws.publish(&r) != astrocs::phase3::P3_OUT_OK);    // 缺 COVERAGE 成员 ⇒ 必拒
        CHECK(!ws.published());
        CHECK(!std::filesystem::exists(std::filesystem::path(p1)));
        ws.abort();
      }
      // D2
      {
        astrocs::phase3::P3FitsStream ws;
        CHECK(ws.open(p2.c_str(), &wcs, w, h, -32, "ADU", &prov) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(1) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.write_block(0, 0, w, h, cov.data()) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(1) != astrocs::phase3::P3_OUT_OK);   // 重复 COVERAGE ⇒ 必拒
        astrocs::phase3::P3OutputResult r{};
        CHECK(ws.publish(&r) != astrocs::phase3::P3_OUT_OK);
        CHECK(!ws.published());
        CHECK(!std::filesystem::exists(std::filesystem::path(p2)));
        ws.abort();
      }
      // D3（正例对照：证明 D1/D2 的"必拒"不是恒真拒绝）
      {
        astrocs::phase3::P3FitsStream ws;
        CHECK(ws.open(p3.c_str(), &wcs, w, h, -32, "ADU", &prov) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.begin_hdu(1) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.write_block(0, 0, w, h, cov.data()) == astrocs::phase3::P3_OUT_OK);
        CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
        astrocs::phase3::P3OutputResult r{};
        CHECK(ws.publish(&r) == astrocs::phase3::P3_OUT_OK);
        std::remove(p3.c_str());
      }
    }

    // ---- 臂 C2: unc 成对序门（IVAR 先于已收尾的 VARIANCE，独立流）----
    {
      const std::string pro077d = dir + "/astrocs_p3_out_test_pr077d.fits";
      std::remove(pro077d.c_str());
      astrocs::phase3::P3FitsStream ws;
      CHECK(ws.open(pro077d.c_str(), &wcs, w, h, -32, "ADU", &prov) ==
            astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(0) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, sig.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(1) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.write_block(0, 0, w, h, cov.data()) == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.end_hdu() == astrocs::phase3::P3_OUT_OK);
      CHECK(ws.begin_hdu(3) != astrocs::phase3::P3_OUT_OK);   // 序门必拒
      astrocs::phase3::P3OutputResult r{};
      CHECK(ws.publish(&r) != astrocs::phase3::P3_OUT_OK);    // 失败后恒拒发布
      CHECK(!ws.published());
      CHECK(!std::filesystem::exists(std::filesystem::path(pro077d)));
      ws.abort();
      no_residue("astrocs_p3_out_test_pr077d.fits");
    }
  }

  // 3e) P-076 (台账 A2) 失败路径不变量: 任一 remove_file 之前必须已 fits_close_file。
  // 旧缺陷形态（bitpix 非法 / create_img 失败 / BUNIT 表外 / 取消 四条失败路径先
  // remove 后 close）在 Windows 上 _unlink 对仍打开的句柄必败 ⇒ tmp 残留；全平台
  // 亦泄漏 fd。本不动量判据在 Linux 上同样可执行：tmp 名是**确定性**的
  // <out>.<pid>.tmp（p3_output.cpp:151-157），故每条失败路径跑完后都能逐名断言
  // "未发布产物 + tmp 不残留"；若实现把 remove 置于 close 之前，Windows 上这些
  // 断言必红（Linux 上该顺序仍能删掉名字，故本门在 Linux 是必要非充分——
  // Windows 节点的复跑由台账 P-076 单独跟踪）。
  {
    const std::string tp = dir + "/astrocs_p3_out_test_pr076.fits";
    const std::string ttmp = tp + "." + std::to_string(P3TEST_GETPID()) + ".tmp";
    std::error_code ecd;
    auto no_file = [&](const std::string& p) {
      return !std::filesystem::exists(std::filesystem::path(p), ecd);
    };
    // (a) bitpix 非法 → PARAM，无产物无 tmp
    {
      std::remove(tp.c_str());
      astrocs::phase3::P3OutputResult r{};
      const astrocs::phase3::P3OutputStatus st =
          astrocs::phase3::p3_output_write_atomic(sig.data(), cov.data(), w, h, &wcs,
                                                  "ADU", tp.c_str(), &prov, -16, -1, &r);
      CHECK(st == astrocs::phase3::P3_OUT_PARAM);
      CHECK(no_file(tp));
      CHECK(no_file(ttmp));
    }
    // (c) 表外 BUNIT（unc 面）→ PARAM，无产物无 tmp
    {
      std::remove(tp.c_str());
      std::vector<float> var(static_cast<size_t>(w) * h, 4.0f);
      std::vector<float> ivar(static_cast<size_t>(w) * h, 0.25f);
      astrocs::phase3::P3OutputResult r{};
      const astrocs::phase3::P3OutputStatus st =
          astrocs::phase3::p3_output_write_atomic_ex(sig.data(), cov.data(), var.data(),
                                                     ivar.data(), w, h, &wcs, "Jy/beam",
                                                     tp.c_str(), &prov, -32, -1, &r);
      CHECK(st == astrocs::phase3::P3_OUT_PARAM);
      CHECK(no_file(tp));
      CHECK(no_file(ttmp));
    }
    // (d) 取消（cancelled_at_row>=0，signal 写出后）→ CANCELLED，无产物无 tmp
    {
      std::remove(tp.c_str());
      astrocs::phase3::P3OutputResult r{};
      const astrocs::phase3::P3OutputStatus st =
          astrocs::phase3::p3_output_write_atomic(sig.data(), cov.data(), w, h, &wcs,
                                                  "ADU", tp.c_str(), &prov, -32, 8, &r);
      CHECK(st == astrocs::phase3::P3_OUT_CANCELLED);
      CHECK(no_file(tp));
      CHECK(no_file(ttmp));
    }
    // 收敛自检: 正常写一次后 tmp 仍不残留（正例，证明上面三条不是"恒真"）
    {
      std::remove(tp.c_str());
      astrocs::phase3::P3OutputResult r{};
      CHECK(astrocs::phase3::p3_output_write_atomic(sig.data(), cov.data(), w, h, &wcs,
                                                    "ADU", tp.c_str(), &prov, -32, -1,
                                                    &r) == astrocs::phase3::P3_OUT_OK);
      CHECK(no_file(ttmp));
      std::remove(tp.c_str());
    }
  }

  // 4) pixel→sky→sample Oracle: WCS roundtrip 后采样信号一致
  {
    double ra, dec, x, y;
    CHECK(astrocs::phase3::p3_wcs_pix2world(&wcs, 32.0, 24.0, &ra, &dec) == astrocs::phase3::P3_WCS_OK);
    CHECK(astrocs::phase3::p3_wcs_world2pix(&wcs, ra, dec, &x, &y) == astrocs::phase3::P3_WCS_OK);
    CHECK(std::fabs(x - 32.0) < 1e-4 && std::fabs(y - 24.0) < 1e-4);
    // 采样值 = 信号 (Oracle: 像素→sky→像素 不变)
    double s = sig[static_cast<size_t>(24) * w + 32];
    CHECK(std::fabs(s - (100.0 + 0.5 * 32)) < 1e-3);
  }

  if (failures == 0) {
    std::printf("P3-005 TESTS PASS (FITS 原子写, 重开 verify dims/WCS/BUNIT/checksum/mask, Oracle roundtrip)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-005 TESTS FAIL (%d)\n", failures);
  return 1;
}
