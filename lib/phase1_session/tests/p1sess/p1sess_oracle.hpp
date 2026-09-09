// P1-SESSION-TEST · 独立 oracle (期望值非被测函数生成, 不调用被测函数)
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// 合同锚: lib/phase1_session/README.md §2 DAG 节点表 (canonical 4 节点
// io_read→calibrate→cosmetic→io_write) + §3 装配顺序 (行号锚实测) +
// §5 错误与取消传播表。
//
// oracle 原则 (MODULE_MIGRATION_TEMPLATE §<prefix>-TEST 第 3 条):
//   oracle 不调用被测函数 (p1_session_* 五导出), 不复制同一实现 ——
//   期望值用独立代数式 + 独立 FITS 读回 (手写大端解码, 非 aio_read)。
//   算法委托面 (ac_calibrate_frame/ac_correct_frame) 的期望值由 ALG-CAL
//   冻结公式独立重算: ALG-CAL-001 标准模式 out = (light - dark)/flat,
//   flat 下限 max(flat,0.1) (lib/calibration/src/calibrator.cpp:110-128
//   实测数学, SCI 权威 docs/algorithms/CALIBRATION_ALGORITHMS.md)。
//   常数场 fixture → 期望 = (light - dark)/max(flat,0.1) 单值代数。
#ifndef P1SESS_ORACLE_HPP
#define P1SESS_ORACLE_HPP

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace p1sess {

// ---------------------------------------------------------------------------
// 独立 FITS 只读解码器 (oracle 通道; 手写大端 float32, 不调用 aio_read)
// 仅解析 oracle 自写的 6 卡头; 返回 false = 文件不可解析 (结构断言用)
// ---------------------------------------------------------------------------
inline bool oracle_read_fits_f32(const std::string& path, int* w, int* h,
                                 std::vector<float>* px) {
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    char card[80];
    auto read_card = [&](char key[9]) -> bool {
        if (std::fread(card, 1, 80, fp) != 80) return false;
        std::memcpy(key, card, 8);
        key[8] = '\0';
        // 去尾部空格
        for (int i = 7; i >= 0; --i)
            if (key[i] == ' ') key[i] = '\0';
            else break;
        return true;
    };
    char key[9];
    if (!read_card(key) || std::strcmp(key, "SIMPLE") != 0) { std::fclose(fp); return false; }
    if (!read_card(key) || std::strcmp(key, "BITPIX") != 0) { std::fclose(fp); return false; }
    if (std::atoi(card + 10) != -32) { std::fclose(fp); return false; }
    if (!read_card(key) || std::strcmp(key, "NAXIS") != 0) { std::fclose(fp); return false; }
    if (std::atoi(card + 10) != 2) { std::fclose(fp); return false; }
    if (!read_card(key) || std::strcmp(key, "NAXIS1") != 0) { std::fclose(fp); return false; }
    const int ww = std::atoi(card + 10);
    if (!read_card(key) || std::strcmp(key, "NAXIS2") != 0) { std::fclose(fp); return false; }
    const int hh = std::atoi(card + 10);
    if (!read_card(key) || std::strcmp(key, "END") != 0) { std::fclose(fp); return false; }
    // 跳到 2880 对齐的数据块起点 (oracle 只认自己写的布局: 头恰 6 卡)
    const long data_start = 2880;
    if (std::fseek(fp, data_start, SEEK_SET) != 0) { std::fclose(fp); return false; }
    const std::size_t n = static_cast<std::size_t>(ww) * static_cast<std::size_t>(hh);
    std::vector<unsigned char> raw(n * 4);
    if (std::fread(raw.data(), 1, raw.size(), fp) != raw.size()) { std::fclose(fp); return false; }
    std::fclose(fp);
    px->resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::uint32_t u = (static_cast<std::uint32_t>(raw[i * 4]) << 24) |
                          (static_cast<std::uint32_t>(raw[i * 4 + 1]) << 16) |
                          (static_cast<std::uint32_t>(raw[i * 4 + 2]) << 8) |
                          static_cast<std::uint32_t>(raw[i * 4 + 3]);
        std::memcpy(&(*px)[i], &u, 4);
    }
    *w = ww;
    *h = hh;
    return true;
}

// ---------------------------------------------------------------------------
// ALG-CAL-001 校准算术 oracle (常数场特化, 独立代数式):
//   标准模式 (dark_opt=0): out = (light - dark) / max(flat, 0.1)
//   优化模式 (dark_opt=1, bias+dark 齐备):
//     out = (light - bias - K*(dark - bias)) / max(flat, 0.1)
//   master 缺省通道由调用方以 has_* 显式声明 (null 指针语义), oracle 内
//   无指针哨兵魔法。常数场 fixture → 期望 = 单值代数式。
//   注: oracle 只服务常数场 fixture (FIX-SESS-A/B), 逐像素通用复算归
//   P1-CAL-TEST (p1cal_oracle) — 本 oracle 验证 session 编排委托, 不重复
//   校准内核验证面。
// ---------------------------------------------------------------------------
inline double oracle_calibrate_const(double light, bool has_dark, double dark,
                                     bool has_flat, double flat) {
    double v = light;
    if (has_dark) v -= dark;
    if (has_flat) v /= std::max(flat, 0.1);
    return v;
}

inline double oracle_calibrate_const_darkopt(double light, double bias,
                                             double dark, double k,
                                             bool has_flat, double flat) {
    double v = light - bias - k * (dark - bias);
    if (has_flat) v /= std::max(flat, 0.1);
    return v;
}

// float 位级比较 (bitwise 容差口径: NaN 布局与 -0.0f 亦须一致)
inline bool bits_eq_f(float a, float b) {
    std::uint32_t ua = 0, ub = 0;
    std::memcpy(&ua, &a, sizeof(ua));
    std::memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}

// f32 相对误差 (存储舍入界; 仅用于乘除舍入链, 常数场代数期望)
inline bool rel_close_f(float got, double want, double rtol) {
    if (std::isnan(want)) return std::isnan(got);
    if (std::isinf(want)) return got == static_cast<float>(want);
    if (want == 0.0) return got == 0.0f;
    const double err = std::fabs(static_cast<double>(got) - want);
    return err <= rtol * std::fabs(want);
}

}  // namespace p1sess

#endif  // P1SESS_ORACLE_HPP
