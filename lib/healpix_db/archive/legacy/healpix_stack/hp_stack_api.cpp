#include "hp_stack_api.h"
#include "stack_db.h"
#include "stack_engine.h"
#include "ahps_reader.h"
#include "ahps_writer.h"
#include "ahps_format.h"
#include "healpix_core.h"

// 梯度校正模块
#include "gradient/gradient_sampler.h"
#include "gradient/gradient_fitter.h"
#include "gradient/corrected_stacker.h"
#include "gradient/snr_evaluator.h"
#include "aio_healpix_io.h"  // hiss_read / hiss_read_snr_model / hcsd_write / hio_free (向后兼容宏)
#include "hp_stack_hiss.h"   // hp_stack_hiss (回退用)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <map>

namespace {

// ============================================================================
// 简单 JSON 解析辅助 (避免外部依赖)
// ============================================================================

size_t skipWs(const std::string& s, size_t pos) {
    while (pos < s.size()) {
        char c = s[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos++;
        else break;
    }
    return pos;
}

// 提取字符串值 (pos 指向 '"')
std::string extractStr(const std::string& s, size_t& pos) {
    std::string r;
    if (pos >= s.size() || s[pos] != '"') return r;
    pos++;
    while (pos < s.size()) {
        char c = s[pos];
        if (c == '\\' && pos + 1 < s.size()) {
            char n = s[pos + 1];
            switch (n) {
                case 'n': r += '\n'; break;
                case 't': r += '\t'; break;
                case 'r': r += '\r'; break;
                case '"': r += '"'; break;
                case '\\': r += '\\'; break;
                default: r += n; break;
            }
            pos += 2;
        } else if (c == '"') {
            pos++;
            break;
        } else {
            r += c;
            pos++;
        }
    }
    return r;
}

// 提取数字 (pos 指向数字起始)
double extractNum(const std::string& s, size_t& pos) {
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') pos++;
    while (pos < s.size()) {
        char c = s[pos];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') pos++;
        else break;
    }
    return std::strtod(s.c_str() + start, nullptr);
}

// 查找 "key": 后的值位置
size_t findKey(const std::string& s, const std::string& key, size_t start = 0) {
    std::string pat = "\"" + key + "\"";
    size_t pos = s.find(pat, start);
    if (pos == std::string::npos) return std::string::npos;
    pos += pat.size();
    pos = skipWs(s, pos);
    if (pos >= s.size() || s[pos] != ':') return std::string::npos;
    pos++;
    return skipWs(s, pos);
}

// 跳过一个 JSON 值 (对象/数组/字符串/数字), 返回值结束后的位置
size_t skipValue(const std::string& s, size_t pos) {
    pos = skipWs(s, pos);
    if (pos >= s.size()) return pos;
    char c = s[pos];
    if (c == '"') {
        // 字符串
        extractStr(s, pos);
        return pos;
    }
    if (c == '{' || c == '[') {
        char open = c, close = (c == '{') ? '}' : ']';
        int depth = 0;
        while (pos < s.size()) {
            char ch = s[pos];
            if (ch == '"') { extractStr(s, pos); continue; }
            if (ch == open) depth++;
            else if (ch == close) { depth--; if (depth == 0) { pos++; break; } }
            pos++;
        }
        return pos;
    }
    // 数字 / true / false / null
    while (pos < s.size()) {
        char ch = s[pos];
        if (ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') break;
        pos++;
    }
    return pos;
}

// 从 DrizzlePixel 对象字符串解析
ahps::DrizzlePixel parseDrizzlePixel(const std::string& objStr) {
    ahps::DrizzlePixel dp;
    dp.healpixPix = 0; dp.value = 0; dp.snr = 0; dp.weight = 1.0f;

    size_t p = findKey(objStr, "healpixPix");
    if (p != std::string::npos) {
        size_t pp = p;
        dp.healpixPix = (uint64_t)extractNum(objStr, pp);
    }
    p = findKey(objStr, "value");
    if (p != std::string::npos) { size_t pp = p; dp.value = (float)extractNum(objStr, pp); }
    p = findKey(objStr, "snr");
    if (p != std::string::npos) { size_t pp = p; dp.snr = (float)extractNum(objStr, pp); }
    p = findKey(objStr, "weight");
    if (p != std::string::npos) { size_t pp = p; dp.weight = (float)extractNum(objStr, pp); }
    return dp;
}

// 从 pixels 数组字符串解析多个 DrizzlePixel
std::vector<ahps::DrizzlePixel> parsePixelArray(const std::string& s, size_t arrPos) {
    std::vector<ahps::DrizzlePixel> result;
    if (arrPos >= s.size() || s[arrPos] != '[') return result;
    size_t pos = arrPos + 1;
    pos = skipWs(s, pos);
    while (pos < s.size() && s[pos] != ']') {
        if (s[pos] == '{') {
            // 匹配 {...}
            int depth = 0;
            size_t start = pos;
            while (pos < s.size()) {
                char c = s[pos];
                if (c == '"') { extractStr(s, pos); continue; }
                if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) { pos++; break; } }
                pos++;
            }
            std::string objStr = s.substr(start, pos - start);
            result.push_back(parseDrizzlePixel(objStr));
        } else {
            pos++;
        }
        pos = skipWs(s, pos);
        if (pos < s.size() && s[pos] == ',') { pos++; pos = skipWs(s, pos); }
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// C API 实现
// ============================================================================

// ---- 数据库管理 ----

HP_STACK_EXPORT StackDatabase* hp_stack_db_create(const char* dbPath, const char* configJson) {
    if (!dbPath) return nullptr;
    ahps::StackDbConfig config;
    if (configJson) {
        std::string json(configJson);
        double v;
        if (findKey(json, "nsideData") != std::string::npos) {
            size_t p = findKey(json, "nsideData");
            config.nsideData = (int)extractNum(json, p);
        }
        if (findKey(json, "tileNside") != std::string::npos) {
            size_t p = findKey(json, "tileNside");
            config.tileNside = (int)extractNum(json, p);
        }
        if (findKey(json, "sigmaClipLow") != std::string::npos) {
            size_t p = findKey(json, "sigmaClipLow");
            config.sigmaClipLow = extractNum(json, p);
        }
        if (findKey(json, "sigmaClipHigh") != std::string::npos) {
            size_t p = findKey(json, "sigmaClipHigh");
            config.sigmaClipHigh = extractNum(json, p);
        }
        if (findKey(json, "nested") != std::string::npos) {
            size_t p = findKey(json, "nested");
            // true/false
            if (p + 4 <= json.size() && std::strncmp(json.c_str() + p, "true", 4) == 0)
                config.nested = true;
            else if (p + 5 <= json.size() && std::strncmp(json.c_str() + p, "false", 5) == 0)
                config.nested = false;
        }
        // bands 数组
        size_t bp = findKey(json, "bands");
        if (bp != std::string::npos && bp < json.size() && json[bp] == '[') {
            config.bands.clear();
            size_t pos = bp + 1;
            pos = skipWs(json, pos);
            while (pos < json.size() && json[pos] != ']') {
                if (json[pos] == '"') {
                    std::string name = extractStr(json, pos);
                    if (!name.empty()) config.bands.push_back(name);
                } else {
                    pos++;
                }
                pos = skipWs(json, pos);
                if (pos < json.size() && json[pos] == ',') { pos++; pos = skipWs(json, pos); }
            }
        }
        // nsideLod 数组
        size_t lp = findKey(json, "nsideLod");
        if (lp != std::string::npos && lp < json.size() && json[lp] == '[') {
            config.nsideLod.clear();
            size_t pos = lp + 1;
            pos = skipWs(json, pos);
            while (pos < json.size() && json[pos] != ']') {
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',')) pos++;
                if (pos >= json.size() || json[pos] == ']') break;
                config.nsideLod.push_back((int)extractNum(json, pos));
                while (pos < json.size() && json[pos] != ',' && json[pos] != ']') pos++;
            }
        }
    }
    ahps::StackDatabase* db = ahps::StackDatabase::create(dbPath, config);
    return reinterpret_cast<StackDatabase*>(db);
}

HP_STACK_EXPORT StackDatabase* hp_stack_db_open(const char* dbPath) {
    if (!dbPath) return nullptr;
    ahps::StackDatabase* db = ahps::StackDatabase::open(dbPath);
    return reinterpret_cast<StackDatabase*>(db);
}

HP_STACK_EXPORT void hp_stack_db_close(StackDatabase* db) {
    if (!db) return;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);
    delete d;
}

// ---- 堆栈更新 ----

HP_STACK_EXPORT int hp_stack_update_global(StackDatabase* db, const char* framesJson) {
    if (!db || !framesJson) return -1;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);
    std::string json(framesJson);

    // 解析 JSON 数组: [{"pixels":[...]}, {"pixels":[...]}, ...]
    std::vector<std::vector<ahps::DrizzlePixel>> frameData;

    size_t pos = skipWs(json, 0);
    if (pos >= json.size() || json[pos] != '[') {
        fprintf(stderr, "[hp_stack] update_global: framesJson 不是数组\n");
        return -1;
    }
    pos++;
    pos = skipWs(json, pos);
    while (pos < json.size() && json[pos] != ']') {
        if (json[pos] == '{') {
            int depth = 0;
            size_t start = pos;
            while (pos < json.size()) {
                char c = json[pos];
                if (c == '"') { extractStr(json, pos); continue; }
                if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) { pos++; break; } }
                pos++;
            }
            std::string frameObj = json.substr(start, pos - start);
            // 解析 pixels 数组
            size_t pp = findKey(frameObj, "pixels");
            if (pp != std::string::npos) {
                auto pixels = parsePixelArray(frameObj, pp);
                frameData.push_back(std::move(pixels));
            }
        } else {
            pos++;
        }
        pos = skipWs(json, pos);
        if (pos < json.size() && json[pos] == ',') { pos++; pos = skipWs(json, pos); }
    }

    ahps::StackEngine engine(d->getConfig());
    return engine.updateGlobal(d, frameData);
}

HP_STACK_EXPORT int hp_stack_update_range(StackDatabase* db, const char* fileRangeJson) {
    if (!db || !fileRangeJson) return -1;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);
    std::string json(fileRangeJson);

    // 解析 JSON 对象: {"filepath1":[pixels...], "filepath2":[pixels...], ...}
    std::map<std::string, std::vector<ahps::DrizzlePixel>> fileRange;

    size_t pos = skipWs(json, 0);
    if (pos >= json.size() || json[pos] != '{') {
        fprintf(stderr, "[hp_stack] update_range: fileRangeJson 不是对象\n");
        return -1;
    }
    pos++;
    pos = skipWs(json, pos);
    while (pos < json.size() && json[pos] != '}') {
        // key (文件路径字符串)
        if (json[pos] != '"') { pos++; continue; }
        std::string key = extractStr(json, pos);
        pos = skipWs(json, pos);
        if (pos >= json.size() || json[pos] != ':') { pos++; continue; }
        pos++;
        pos = skipWs(json, pos);
        // value (pixels 数组)
        if (pos < json.size() && json[pos] == '[') {
            auto pixels = parsePixelArray(json, pos);
            fileRange[key] = std::move(pixels);
            pos = skipValue(json, pos);
        } else {
            pos = skipValue(json, pos);
        }
        pos = skipWs(json, pos);
        if (pos < json.size() && json[pos] == ',') { pos++; pos = skipWs(json, pos); }
    }

    ahps::StackEngine engine(d->getConfig());
    return engine.updateRange(d, fileRange);
}

// ---- 读取堆栈数据 ----

HP_STACK_EXPORT char* hp_stack_read_tile(StackDatabase* db, int64_t tileIpix) {
    if (!db) return nullptr;
    ahps::StackDatabase* d = reinterpret_cast<ahps::StackDatabase*>(db);

    ahps::AhpsReader* reader = d->openTileReader(tileIpix);
    if (!reader) return nullptr;

    auto pixels = reader->readPixelIndices();
    int bandCount = reader->getBandCount();
    int64_t pixCount = reader->getPixelCount();

    // 构建 JSON
    std::string json = "{";
    char hdr[256];
    std::snprintf(hdr, sizeof(hdr),
        "\"tileIpix\":%lld,\"nside\":%d,\"tileNside\":%d,\"pixelCount\":%lld,\"bandCount\":%d,",
        (long long)tileIpix, reader->getNside(), reader->getTileNside(),
        (long long)pixCount, bandCount);
    json += hdr;

    // pixels 数组
    json += "\"pixels\":[";
    for (size_t i = 0; i < pixels.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(pixels[i]);
    }
    json += "],";

    // bands 数组
    json += "\"bands\":[";
    for (int b = 0; b < bandCount; b++) {
        if (b > 0) json += ",";
        std::vector<float> values, variance;
        auto stats = reader->readBandStats(b);
        json += "{\"values\":[";
        for (size_t i = 0; i < stats.size(); i++) {
            if (i > 0) json += ",";
            float val = (stats[i].weightSum > 0.0f) ? (stats[i].sum / stats[i].weightSum) : 0.0f;
            json += std::to_string(val);
        }
        json += "],\"variance\":[";
        for (size_t i = 0; i < stats.size(); i++) {
            if (i > 0) json += ",";
            float var = 0.0f;
            if (stats[i].weightSum > 0.0f) {
                float val = stats[i].sum / stats[i].weightSum;
                float e2 = stats[i].sumSq / stats[i].weightSum;
                var = e2 - val * val;
                if (var < 0.0f) var = 0.0f;
            }
            json += std::to_string(var);
        }
        json += "],\"counts\":[";
        for (size_t i = 0; i < stats.size(); i++) {
            if (i > 0) json += ",";
            json += std::to_string((int)stats[i].count);
        }
        json += "]}";
    }
    json += "]}";

    delete reader;

    // malloc 分配并拷贝
    char* out = (char*)std::malloc(json.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, json.c_str(), json.size() + 1);
    return out;
}

HP_STACK_EXPORT void hp_stack_free_string(char* str) {
    if (str) std::free(str);
}

// ============================================================================
// Stack 阶段 PipelineFrame 入口实现
// 接收多帧 PipelineFrame, 堆栈后输出单个 .ahps 文件
//
// 注意: PipelineFrame 已迁移为纯命名块容器 (aio_pipeline.h),
//       旧版字段 healpix_pixels/healpix_ipix/nside/nested/n_healpix 已移除。
//       该入口已废弃, 请改用 hp_stack_hiss (.hiss → .hcsd 路径)。
// ============================================================================
HP_STACK_EXPORT int hp_stack_run(const PipelineFrame** frames, int n_frames,
                                  const char* output_path, HpStackResult* result) {
    if (result) {
        result->pixel_count = 0;
        result->tile_count = 0;
        result->success = 0;
    }
    (void)frames; (void)n_frames; (void)output_path;
    fprintf(stderr, "[hp_stack_run] 已废弃: PipelineFrame 已迁移为命名块容器。\n"
                    "  请改用 hp_stack_hiss (.hiss → .hcsd 路径)。\n");
    return 1;
}

// ============================================================================
// 梯度校正叠加 (.hiss → .hcsd, 含球面 TPS 梯度校正)
//
// 完整流程: 采样 → Gauss-Seidel 拟合 → 校正叠加 → .hcsd 输出
// 内部: gradient_sampler + gradient_fitter + corrected_stacker
// ============================================================================
HP_STACK_EXPORT int hp_stack_gradient_corrected(
    const char** hiss_paths, int n_frames,
    const char* gaia_data_dir,
    const char* output_hcsd_path,
    double sigma, int max_iter,
    int gradient_max_iter, double gradient_lambda,
    const char* sigma_clip_method,
    double winsorize_low_pct,
    double winsorize_high_pct)
{
    // ---- 参数校验 ----
    if (!hiss_paths || n_frames <= 0 || !output_hcsd_path) {
        fprintf(stderr, "[hp_stack_gradient_corrected] 无效参数 "
                "(paths=%p n_frames=%d out=%p)\n",
                (const void*)hiss_paths, n_frames, (const void*)output_hcsd_path);
        return -1;
    }
    if (sigma <= 0.0) sigma = 3.0;
    if (max_iter <= 0) max_iter = 5;
    if (gradient_max_iter <= 0) gradient_max_iter = 10;
    if (gradient_lambda <= 0.0) gradient_lambda = 1e-4;

    // GAP-017: Winsorized sigma clip 参数处理
    // sigma_clip_method=nullptr 或 "standard" → 普通 sigma-clip (向后兼容)
    // sigma_clip_method="winsorized"           → Winsorized sigma clip
    bool use_winsorized = false;
    if (sigma_clip_method && std::string(sigma_clip_method) == "winsorized") {
        use_winsorized = true;
        if (winsorize_low_pct  <= 0.0 || winsorize_low_pct  >= 1.0) winsorize_low_pct  = 0.05;
        if (winsorize_high_pct <= 0.0 || winsorize_high_pct >= 1.0) winsorize_high_pct = 0.95;
        if (winsorize_low_pct >= winsorize_high_pct) {
            winsorize_low_pct = 0.05;
            winsorize_high_pct = 0.95;
        }
    }

    fprintf(stderr, "[hp_stack_gradient_corrected] 开始: %d 帧 → %s "
            "(sigma=%.2f, max_iter=%d, grad_iter=%d, grad_lambda=%.1e, "
            "sigma_clip=%s, winsor_low=%.2f, winsor_high=%.2f)\n",
            n_frames, output_hcsd_path, sigma, max_iter,
            gradient_max_iter, gradient_lambda,
            use_winsorized ? "winsorized" : "standard",
            use_winsorized ? winsorize_low_pct : 0.0,
            use_winsorized ? winsorize_high_pct : 0.0);

    // ---- 阶段 1: 球面背景采样 ----
    fprintf(stderr, "[hp_stack_gradient_corrected] === 阶段1: 球面背景采样 ===\n");
    std::vector<gradient::FrameInfo> frame_infos(n_frames);
    for (int f = 0; f < n_frames; ++f) {
        frame_infos[f].hiss_path = hiss_paths[f] ? hiss_paths[f] : "";
        frame_infos[f].frame_id  = f;
    }

    gradient::SamplerParams sampler_params;
    gradient::GradientSampler sampler;
    gradient::SampleResult sample_result;

    const char* gaia_dir = gaia_data_dir ? gaia_data_dir : "";
    int rc = sampler.sample(frame_infos.data(), n_frames,
                            gaia_dir, sampler_params, sample_result);
    if (rc != 0 || sample_result.rows.empty()) {
        fprintf(stderr, "[hp_stack_gradient_corrected] 采样失败 (rc=%d, rows=%zu), "
                "回退到无梯度校正模式\n", rc, sample_result.rows.size());
        // 回退: 直接调用 hp_stack_hiss (无梯度校正)
        return hp_stack_hiss(hiss_paths, n_frames, output_hcsd_path,
                             sigma, max_iter);
    }

    fprintf(stderr, "[hp_stack_gradient_corrected] 采样完成: %d 样本, %d 帧处理\n",
            sample_result.total_samples, sample_result.n_frames_processed);

    // ---- 阶段 2: 差异拟合 (3D 嵌入球面样条, 无迭代) ----
    fprintf(stderr, "[hp_stack_gradient_corrected] === 阶段2: 差异拟合 (3D 嵌入球面样条) ===\n");
    gradient::FitterParams fitter_params;
    fitter_params.lambda     = gradient_lambda;
    fitter_params.enable_gauge_fixing = true;

    gradient::GradientFitter fitter;
    gradient::FitterResult fitter_result;

    rc = fitter.fit(sample_result.rows.data(),
                    (int)sample_result.rows.size(),
                    sample_result.frame_ids.data(),
                    (int)sample_result.frame_ids.size(),
                    fitter_params, fitter_result);
    if (rc != 0) {
        fprintf(stderr, "[hp_stack_gradient_corrected] 拟合失败 (rc=%d), "
                "回退到无梯度校正模式\n", rc);
        return hp_stack_hiss(hiss_paths, n_frames, output_hcsd_path,
                             sigma, max_iter);
    }

    fprintf(stderr, "[hp_stack_gradient_corrected] 差异拟合完成: success=%d, "
            "lambda=%.6e\n",
            fitter_result.success ? 1 : 0,
            gradient_lambda);

    // 诊断: 输出每帧样条模型的 w_k 和 v 值范围
    for (size_t i = 0; i < fitter_result.models.size(); ++i) {
        const auto& m = fitter_result.models[i];
        if (!m.valid) {
            fprintf(stderr, "[hp_stack_gradient_corrected] 帧 %zd: model invalid\n", i);
            continue;
        }
        double w_min =  std::numeric_limits<double>::max();
        double w_max = -std::numeric_limits<double>::max();
        double w_abs_max = 0.0;
        for (double w : m.weights) {
            if (w < w_min) w_min = w;
            if (w > w_max) w_max = w;
            if (std::fabs(w) > w_abs_max) w_abs_max = std::fabs(w);
        }
        fprintf(stderr, "[hp_stack_gradient_corrected] 帧 %zd: n_ctrl=%d "
                "w_range=[%.4f, %.4f] w_absmax=%.4f "
                "v=[%.4f, %.4f, %.4f, %.4f] fit_rms=%.4f\n",
                i, (int)m.weights.size(),
                w_min, w_max, w_abs_max,
                m.v[0], m.v[1], m.v[2], m.v[3], m.fit_rms);
    }

    // ---- 阶段 3: 逐帧读取 .hiss, 重建逐像素 SNR, 构造 FrameData ----
    fprintf(stderr, "[hp_stack_gradient_corrected] === 阶段3: 读取帧数据 + SNR 重建 ===\n");
    std::vector<gradient::FrameData> frame_data(n_frames);
    uint32_t nside_out = 0;
    int nested_out = 0;
    std::string filter_str;
    double total_exposure = 0.0;

    // 构建 frame_id → model 索引映射
    // fitter_result.models 索引对应 sample_result.frame_ids
    // frame_data[f].frame_id = f, 需找到对应的 model
    // 由于 sample_result.frame_ids 可能是子集, 需映射
    std::map<int32_t, int> frame_id_to_model_idx;
    for (size_t i = 0; i < fitter_result.frame_ids.size(); ++i) {
        frame_id_to_model_idx[fitter_result.frame_ids[i]] = (int)i;
    }

    for (int f = 0; f < n_frames; ++f) {
        if (!hiss_paths[f]) {
            fprintf(stderr, "[hp_stack_gradient_corrected] 帧 %d 路径为空, 跳过\n", f);
            continue;
        }

        // 读取 .hiss (含 ipix + pixel)
        uint32_t nside_f = 0;
        int nested_f = 0;
        uint64_t n_pix_f = 0;
        uint64_t* ipix_f = nullptr;
        float* pixel_f = nullptr;
        char* meta_f = nullptr;

        rc = hiss_read(hiss_paths[f], &nside_f, &nested_f, &n_pix_f,
                       &ipix_f, &pixel_f, nullptr, &meta_f);
        if (rc != 0) {
            fprintf(stderr, "[hp_stack_gradient_corrected] 帧 %d hiss_read 失败 (rc=%d): %s\n",
                    f, rc, hiss_paths[f]);
            return -5;
        }

        // 校验 nside/nested 一致性
        if (nside_out == 0) {
            nside_out  = nside_f;
            nested_out = nested_f;
        } else if (nside_f != nside_out || nested_f != nested_out) {
            fprintf(stderr, "[hp_stack_gradient_corrected] nside/nested 不一致: 帧 %d "
                    "(%u/%d) vs 首帧 (%u/%d)\n",
                    f, nside_f, nested_f, nside_out, nested_out);
            if (ipix_f) hio_free(ipix_f);
            if (pixel_f) hio_free(pixel_f);
            if (meta_f) hio_free(meta_f);
            return -3;
        }

        // 提取元数据
        if (meta_f) {
            std::string meta_str(meta_f);
            if (filter_str.empty()) {
                // 简单提取 "filter":"xxx"
                size_t pos = meta_str.find("\"filter\"");
                if (pos != std::string::npos) {
                    pos = meta_str.find('"', pos + 8);
                    if (pos != std::string::npos) {
                        size_t start = pos + 1;
                        size_t end = meta_str.find('"', start);
                        if (end != std::string::npos) {
                            filter_str = meta_str.substr(start, end - start);
                        }
                    }
                }
            }
            // 提取 exposure_s
            size_t epos = meta_str.find("\"exposure_s\"");
            if (epos != std::string::npos) {
                epos = meta_str.find(':', epos + 12);
                if (epos != std::string::npos) {
                    total_exposure += std::strtod(meta_f + epos + 1, nullptr);
                }
            }
        }

        // 读取稀疏 SNR 模型 → SnrEvaluator 重建逐像素 SNR
        uint32_t snside = 0;
        int snested = 0;
        uint64_t snpix = 0;
        uint64_t* sipix = nullptr;
        float* spixel = nullptr;
        HioSnrModel* snr_model = nullptr;
        char* smeta = nullptr;

        gradient::SnrEvaluator snr_eval;
        bool snr_built = false;

        rc = hiss_read_snr_model(hiss_paths[f], &snside, &snested, &snpix,
                                 &sipix, &spixel, &snr_model, &smeta);
        if (rc == 0 && snr_model && snr_model->n_points > 0) {
            std::vector<double> cp_ra(snr_model->n_points);
            std::vector<double> cp_dec(snr_model->n_points);
            std::vector<float>  cp_snr(snr_model->n_points);
            for (uint32_t i = 0; i < snr_model->n_points; ++i) {
                cp_ra[i]  = snr_model->points[i].ra;
                cp_dec[i] = snr_model->points[i].dec;
                cp_snr[i] = snr_model->points[i].snr_psf;
            }
            double idw_p = (snr_model->idw_power > 0)
                           ? snr_model->idw_power : 2.0;
            snr_built = snr_eval.build(snr_model->n_points,
                                       cp_ra.data(), cp_dec.data(),
                                       cp_snr.data(),
                                       snr_model->snr_phot,
                                       snr_model->median_snr, idw_p);
        }

        // 释放 snr_model 读取资源
        if (sipix) hio_free(sipix);
        if (spixel) hio_free(spixel);
        if (snr_model) hio_free_snr_model(snr_model);
        if (smeta) hio_free(smeta);

        // 构造 FrameData
        frame_data[f].frame_id = f;
        frame_data[f].ipix.assign(ipix_f, ipix_f + n_pix_f);
        frame_data[f].pixel.assign(pixel_f, pixel_f + n_pix_f);

        // 逐像素 SNR 重建 (若 snr_built)
        if (snr_built && n_pix_f > 0) {
            frame_data[f].snr.resize(n_pix_f);
            // 批量评估: ipix → (ra,dec) → SNR
            std::vector<double> ra_arr(n_pix_f), dec_arr(n_pix_f);
            healpix::HealpixCore core((int)nside_f, nested_f != 0);
            for (uint64_t i = 0; i < n_pix_f; ++i) {
                core.pix2radec(ipix_f[i], &ra_arr[i], &dec_arr[i]);
            }
            snr_eval.evaluateBatch(ra_arr.data(), dec_arr.data(),
                                   n_pix_f, frame_data[f].snr.data());
        }
        // 若 snr 未重建, snr 为空 → corrected_stacker 使用等权 (w=1.0)

        // 释放 hiss_read 资源
        if (ipix_f) hio_free(ipix_f);
        if (pixel_f) hio_free(pixel_f);
        if (meta_f) hio_free(meta_f);

        fprintf(stderr, "[hp_stack_gradient_corrected] 帧 %d: n_pix=%llu, snr_built=%d\n",
                f, (unsigned long long)n_pix_f, snr_built ? 1 : 0);
    }

    if (nside_out == 0) {
        fprintf(stderr, "[hp_stack_gradient_corrected] 无有效数据\n");
        return -6;
    }

    // ---- 阶段 4: 梯度校正叠加 ----
    fprintf(stderr, "[hp_stack_gradient_corrected] === 阶段4: 梯度校正叠加 ===\n");

    // 构建 model 数组: 按 frame_data 的 frame_id 索引
    // corrected_stacker 期望 models[frame_id] 对应 frame_data[f].frame_id
    // frame_data[f].frame_id = f, 所以 models 数组按 f 索引
    // 但 fitter_result.models 按 sample_result.frame_ids 索引
    // 需要重映射: models_out[f] = fitter_result.models[frame_id_to_model_idx[f]]
    int max_frame_id = 0;
    for (int f = 0; f < n_frames; ++f) {
        if (frame_data[f].frame_id > max_frame_id)
            max_frame_id = frame_data[f].frame_id;
    }
    std::vector<gradient::SplineModel> models_out(max_frame_id + 1);
    for (int f = 0; f < n_frames; ++f) {
        int32_t fid = frame_data[f].frame_id;
        auto it = frame_id_to_model_idx.find(fid);
        if (it != frame_id_to_model_idx.end() && it->second < (int)fitter_result.models.size()) {
            models_out[fid] = fitter_result.models[it->second];
        }
        // 未找到 model 的帧, models_out[fid].valid = false → 不校正
    }

    gradient::CorrectedStackParams stack_params;
    stack_params.sigma    = sigma;
    stack_params.max_iter = max_iter;
    stack_params.nside    = (int)nside_out;
    stack_params.nested   = nested_out != 0;
    // GAP-017: Winsorized sigma clip 参数透传
    if (use_winsorized) {
        stack_params.use_winsorized    = true;
        stack_params.winsorize_low_pct = winsorize_low_pct;
        stack_params.winsorize_high_pct = winsorize_high_pct;
    }

    gradient::CorrectedStacker stacker;
    gradient::StackResult stack_result;

    rc = stacker.stack(frame_data.data(), n_frames,
                       models_out.data(), (int)models_out.size(),
                       (int)nside_out, nested_out != 0,
                       stack_params, stack_result);
    if (rc != 0) {
        fprintf(stderr, "[hp_stack_gradient_corrected] 校正叠加失败 (rc=%d): %s\n",
                rc, stacker.lastError().c_str());
        return -7;
    }

    fprintf(stderr, "[hp_stack_gradient_corrected] 叠加完成: %zu 像素\n",
            stack_result.ipix.size());

    // ---- 阶段 5: 写入 .hcsd ----
    fprintf(stderr, "[hp_stack_gradient_corrected] === 阶段5: 写入 .hcsd ===\n");

    // 准备输出数组 (uint64_t for hcsd_write, float for pixel)
    std::vector<uint64_t> out_ipix(stack_result.ipix.size());
    for (size_t i = 0; i < stack_result.ipix.size(); ++i) {
        out_ipix[i] = (uint64_t)stack_result.ipix[i];
    }
    std::vector<float> out_pixel(out_ipix.size());
    for (size_t i = 0; i < out_ipix.size(); ++i) {
        out_pixel[i] = (float)stack_result.mean[i];
    }

    // 构建 meta_json (GAP-017: 增加 sigma_clip_method 字段)
    char meta_buf[1024];
    std::snprintf(meta_buf, sizeof(meta_buf),
        "{\"filter\":\"%s\",\"n_frames\":%d,\"total_exposure_s\":%.3f,"
        "\"sigma_clip\":{\"sigma\":%.4f,\"max_iter\":%d,\"method\":\"%s\","
        "\"winsorize_low\":%.4f,\"winsorize_high\":%.4f},"
        "\"gradient_correction\":{\"enabled\":true,\"success\":%s,"
        "\"lambda\":%.1e,\"method\":\"diff_fit_spherical_spline\"}}",
        filter_str.c_str(), n_frames, total_exposure,
        sigma, max_iter,
        use_winsorized ? "winsorized" : "standard",
        use_winsorized ? winsorize_low_pct : 0.0,
        use_winsorized ? winsorize_high_pct : 0.0,
        fitter_result.success ? "true" : "false",
        gradient_lambda);

    rc = hcsd_write(output_hcsd_path, nside_out, nested_out,
                    (uint64_t)out_ipix.size(), out_ipix.data(),
                    out_pixel.data(), meta_buf);
    if (rc != 0) {
        fprintf(stderr, "[hp_stack_gradient_corrected] hcsd_write 失败 (rc=%d)\n", rc);
        return -8;
    }

    fprintf(stderr, "[hp_stack_gradient_corrected] 成功: %s (%zu 像素)\n",
            output_hcsd_path, out_ipix.size());
    return 0;
}

// ---- HEALpix 工具函数 ----

HP_STACK_EXPORT int64_t hp_radec2pix(int nside, int nested, double ra_deg, double dec_deg) {
    healpix::HealpixCore core(nside, nested != 0);
    return core.radec2pix(ra_deg, dec_deg);
}

HP_STACK_EXPORT void hp_pix2radec(int nside, int nested, int64_t ipix,
                                  double* ra_deg, double* dec_deg) {
    healpix::HealpixCore core(nside, nested != 0);
    double ra, dec;
    core.pix2radec(ipix, &ra, &dec);
    if (ra_deg) *ra_deg = ra;
    if (dec_deg) *dec_deg = dec;
}

HP_STACK_EXPORT double hp_pixel_resolution_arcsec(int nside) {
    healpix::HealpixCore core(nside, true);
    return core.pixelResolutionArcsec();
}
