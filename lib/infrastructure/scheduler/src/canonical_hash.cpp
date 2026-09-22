// lib/infrastructure/scheduler/src/canonical_hash.cpp — 规范产品哈希 C++ 实现
//
// 与 eng/tools/canonical_product_hash.py 逐字节同构（同一 spec id、同一域分隔前缀、
// 同一规范文本编码）。规范文本刻意避开语言相关的浮点格式化: 数字一律用
// "%.17g"（C 与 Python 同义）, FITS 卡用**原始 80 字节卡文本**（尾随 0x20/0x00
// 裁剪后）而非重排版, 因此两种实现不存在格式化漂移面。
//
// 依赖: 仅 astrocs::crypto（SHA-256）与 nlohmann::json; **不引入 cfitsio**
//（core 的依赖图不得被产品 IO 库污染, 见 module_adapters 的 astrocs_cfitsio
// 初始化 shim 注释）: FITS 结构用最小头解析（2880 对齐 + BITPIX/NAXIS/PCOUNT/
// GCOUNT）。
#include "astrocs/core/canonical_hash.h"

#include "crypto/sha256.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 文件读取/存在性/
// 头部探测/流式分块读取一律经 aio 唯一实现 (aio_file_io.h / aio_atomic_file.h,
// 均为 header-only 机制面 —— 不引入 cfitsio 或产品 IO 库链接依赖, core 依赖图
// 保持原样)。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

namespace astrocs::core {

const char* const kCanonicalProductHashSpec = "astrocs.canonical-product-hash/v1";

namespace {

const char kDomain[] = "astrocs.canonical-product-hash/v1\n";

const char* const kExcludedFitsCards[] = {"CHECKSUM", "DATASUM", "DATE", "RUNID"};
const char* const kExcludedJsonKeys[] = {
    "run_id",        "elapsed_sec",      "created_utc",     "started_utc",
    "finished_utc",  "ended_utc",        "generated_utc",   "timestamp_utc",
    "hips_creation_date", "hips_update_date", "hips_release_date",
    "sha256",        "product_sha256",   "integrity_sha256"};
const char* const kExcludedPropsKeys[] = {"hips_creation_date", "hips_update_date",
                                          "hips_release_date"};

bool in_list(const char* const* list, std::size_t n, const std::string& key) {
  for (std::size_t i = 0; i < n; ++i)
    if (key == list[i]) return true;
  return false;
}

// 尾随 0x20 / 0x00 裁剪（与 Python 侧 rec.rstrip(b" \x00") 同义）
std::string trim_tail_nul_space(const std::string& s) {
  std::size_t e = s.size();
  while (e > 0 && (s[e - 1] == ' ' || s[e - 1] == '\0')) --e;
  return s.substr(0, e);
}

bool is_blank_card(const std::string& s) {
  for (char c : s)
    if (c != ' ' && c != '\0') return false;
  return true;
}

std::string trim_ws(const std::string& s) {
  std::size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::string read_file_bytes(const std::string& path, bool* ok) {
  // CLEAN-403: 整文件读取经 aio (aio_file::read_all); 失败 ⇒ ok=false 且空串。
  std::string out;
  const bool got = aio_file::read_all(path.c_str(), &out);
  if (ok) *ok = got;
  return got ? out : std::string();
}

// 纯字符串路径取文件名 (无文件系统调用; 与 std::filesystem::path::filename 同义)。
std::string file_name(const std::string& path) {
  const std::size_t s = path.find_last_of("/\\");
  return (s == std::string::npos) ? path : path.substr(s + 1);
}

std::string sha256_str(const std::string& s) {
  return astrocs::crypto::sha256_hex(s.data(), s.size());
}

std::string fits_keyword(const std::string& card) {
  std::string k = card.substr(0, 8);
  std::size_t e = k.size();
  while (e > 0 && (k[e - 1] == ' ' || k[e - 1] == '\0')) --e;
  std::string out = k.substr(0, e);
  for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return out;
}

// FITS 卡的整数值（第 11.. 列, 引号/注释不计）
long long fits_card_int(const std::string& card, long long dflt) {
  std::string body = card.size() > 10 ? card.substr(10) : std::string();
  std::size_t slash = body.find('/');
  if (slash != std::string::npos) body = body.substr(0, slash);
  body = trim_ws(body);
  if (body.empty()) return dflt;
  char* end = nullptr;
  long long v = std::strtoll(body.c_str(), &end, 10);
  if (end == body.c_str()) return dflt;
  return v;
}

std::string fits_card_str(const std::string& card) {
  std::string body = card.size() > 10 ? card.substr(10) : std::string();
  std::size_t slash = body.find('/');
  if (slash != std::string::npos) body = body.substr(0, slash);
  body = trim_ws(body);
  if (body.size() >= 2 && body.front() == '\'' && body.back() == '\'')
    body = body.substr(1, body.size() - 2);
  return trim_ws(body);
}

// ── 规范 JSON 文本（与 Python canonical_json_text 逐字同构）──────────────────
void json_escape(const std::string& s, std::string* out) {
  out->push_back('"');
  for (unsigned char c : s) {
    if (c == '"') out->append("\\\"");
    else if (c == '\\') out->append("\\\\");
    else if (c < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
      out->append(buf);
    } else {
      out->push_back(static_cast<char>(c));
    }
  }
  out->push_back('"');
}

void json_number_text(const nlohmann::json& v, std::string* out) {
  if (v.is_number_unsigned()) {
    *out += std::to_string(v.get<unsigned long long>());
  } else if (v.is_number_integer()) {
    *out += std::to_string(v.get<long long>());
  } else {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v.get<double>());
    *out += buf;
  }
}

void json_canonical_text(const nlohmann::json& v, std::string* out) {
  if (v.is_null()) { *out += "null"; return; }
  if (v.is_boolean()) { *out += v.get<bool>() ? "true" : "false"; return; }
  if (v.is_number()) { json_number_text(v, out); return; }
  if (v.is_string()) { json_escape(v.get<std::string>(), out); return; }
  if (v.is_array()) {
    out->push_back('[');
    bool first = true;
    for (const auto& e : v) {
      if (!first) out->push_back(',');
      first = false;
      json_canonical_text(e, out);
    }
    out->push_back(']');
    return;
  }
  if (v.is_object()) {
    std::vector<std::string> keys;
    keys.reserve(v.size());
    for (auto it = v.begin(); it != v.end(); ++it) keys.push_back(it.key());
    std::sort(keys.begin(), keys.end());  // std::string: UTF-8 字节序
    out->push_back('{');
    bool first = true;
    for (const std::string& k : keys) {
      if (!first) out->push_back(',');
      first = false;
      json_escape(k, out);
      out->push_back(':');
      json_canonical_text(v.at(k), out);
    }
    out->push_back('}');
    return;
  }
  // 不可达（nlohmann 的 6 种类型已覆盖）
  *out += "null";
}

nlohmann::json prune_json(const nlohmann::json& v,
                          const std::vector<std::string>& excluded,
                          std::vector<std::string>* hits, const std::string& prefix) {
  if (v.is_object()) {
    nlohmann::json out = nlohmann::json::object();
    for (auto it = v.begin(); it != v.end(); ++it) {
      const std::string key = it.key();
      if (std::find(excluded.begin(), excluded.end(), key) != excluded.end()) {
        hits->push_back(prefix.empty() ? key : prefix + "/" + key);
        continue;
      }
      out[key] = prune_json(it.value(), excluded, hits,
                            prefix.empty() ? key : prefix + "/" + key);
    }
    return out;
  }
  if (v.is_array()) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& e : v) out.push_back(prune_json(e, excluded, hits, prefix + "[]"));
    return out;
  }
  return v;
}

struct FitsCard {
  std::string keyword;
  std::size_t order;
  std::string text;
};

// 最小 FITS 结构解析: 逐 HDU 取 [头卡, 数据单元] 字节区间（2880 对齐）。
// 返回 false = 结构非法（调用方按失败处理, 不做静默降级）。
// P3-STREAM-01：FITS 规范哈希改为**流式**读取 —— 头部按 2880 字节逻辑记录读、
// 数据单元按 64 KiB 分块喂 SHA-256，峰值内存 = 单块（与文件大小无关）。
// 判据：ASTROCS_DESIGN §8.3 export 行「内存占用与子块大小成正比、与总图大小
// 无关」—— 整文件 read_all 会让 writer/verify 节点的峰值随产品大小线性增长。
// 语义不变：canonical 正文与逐 HDU DATA sha256 与整读实现逐字节相同
//（eng/tools/canonical_product_hash.py 为独立镜像，可交叉核对）。
bool parse_fits_canonical(const char* path, std::uint64_t total, std::string* out,
                          std::vector<std::string>* excluded_hits,
                          std::vector<std::string>* warnings) {
  std::uint64_t pos = 0;
  int hdu = 0;
  while (pos + 80 <= total) {
    std::string first_block;
    if (!aio_file::read_range(path, pos, 80, &first_block)) return false;
    const std::string first = fits_keyword(first_block);
    if (first != "SIMPLE" && first != "XTENSION") {
      if (hdu == 0) return false;  // 不是 FITS
      break;                       // 尾部填充, 正常结束
    }
    std::vector<FitsCard> cards;
    long long bitpix = 0, pcount = 0, gcount = 1;
    std::vector<long long> naxis;
    std::string xtension;
    std::uint64_t off = pos;
    std::size_t order = 0;
    bool ended = false;
    std::string hdr;          // 当前 2880 字节逻辑记录
    std::size_t blk_off = 0;  // 记录内偏移
    while (off + 80 <= total) {
      if (blk_off == 0) {
        const std::uint64_t want =
            std::min<std::uint64_t>(2880, total - off);
        if (!aio_file::read_range(path, off, static_cast<std::size_t>(want), &hdr))
          return false;
      }
      if (blk_off + 80 > hdr.size()) break;
      const std::string card = hdr.substr(blk_off, 80);
      off += 80;
      blk_off += 80;
      if (blk_off >= hdr.size()) blk_off = 0;
      const std::string kw = fits_keyword(card);
      if (kw == "END") { ended = true; break; }
      if (kw == "BITPIX") bitpix = fits_card_int(card, 0);
      else if (kw == "NAXIS") {
        const long long n = fits_card_int(card, 0);
        naxis.assign(n > 0 ? static_cast<std::size_t>(n) : 0u, 0);
      } else if (kw.rfind("NAXIS", 0) == 0 && kw.size() > 5) {
        const long long idx = std::strtoll(kw.c_str() + 5, nullptr, 10);
        if (idx >= 1 && static_cast<std::size_t>(idx) <= naxis.size())
          naxis[static_cast<std::size_t>(idx) - 1] = fits_card_int(card, 0);
      } else if (kw == "PCOUNT") {
        pcount = fits_card_int(card, 0);
      } else if (kw == "GCOUNT") {
        gcount = fits_card_int(card, 1);
      } else if (kw == "XTENSION") {
        xtension = fits_card_str(card);
      }
      if (is_blank_card(card)) continue;
      if (in_list(kExcludedFitsCards,
                  sizeof(kExcludedFitsCards) / sizeof(kExcludedFitsCards[0]), kw)) {
        excluded_hits->push_back("HDU" + std::to_string(hdu) + ":" + kw);
        continue;
      }
      FitsCard fc;
      fc.keyword = kw;
      fc.order = order++;
      fc.text = trim_tail_nul_space(card);
      cards.push_back(std::move(fc));
    }
    if (!ended) return false;
    // 头部长度按 2880 逻辑记录对齐: END 卡之后到本 HDU 起点的余量补满记录边界
    //（标准允许 END 之后为空白填充; 数据单元起点 = 头记录结束处, 而非 END 卡末端）。
    {
      const std::size_t hdr_len = ((off - pos) + 2879u) / 2880u * 2880u;
      off = pos + hdr_len;
      if (off > total) return false;
    }
    // 排序: 按 keyword 升序（等价 Python 的 (keyword, 出现序) 稳定排序）
    std::stable_sort(cards.begin(), cards.end(),
                     [](const FitsCard& a, const FitsCard& c) { return a.keyword < c.keyword; });
    // 数据单元字节数
    long long nbytes = 0;
    const bool is_table = !xtension.empty() && xtension != "IMAGE";
    if (is_table) {
      const long long row = naxis.size() >= 1 ? naxis[0] : 0;
      const long long rows = naxis.size() >= 2 ? naxis[1] : 1;
      nbytes = row * rows * (gcount > 0 ? gcount : 1) + pcount;
    } else {
      long long npix = 1;
      for (long long n : naxis) npix *= n;
      const long long bpp = bitpix < 0 ? (-bitpix) / 8 : bitpix / 8;
      nbytes = npix * bpp * (gcount > 0 ? gcount : 1);
    }
    if (nbytes < 0) return false;
    const std::size_t unit =
        (static_cast<std::size_t>(nbytes) + 2879u) / 2880u * 2880u;
    if (off + unit > total) return false;
    // 数据单元：分块流式 sha256（峰值 = 单块 64 KiB，与文件大小无关）
    astrocs::crypto::Sha256 data_hash;
    {
      std::uint64_t remaining = unit;
      std::uint64_t doff = off;
      while (remaining > 0) {
        const std::size_t want = static_cast<std::size_t>(
            std::min<std::uint64_t>(64 * 1024, remaining));
        std::string chunk;
        if (!aio_file::read_range(path, doff, want, &chunk)) return false;
        if (chunk.empty()) return false;
        data_hash.update(chunk.data(), chunk.size());
        doff += chunk.size();
        remaining -= chunk.size();
      }
    }
    *out += "HDU " + std::to_string(hdu) + "\n";
    for (const FitsCard& fc : cards) {
      *out += "CARD ";
      *out += fc.text;
      *out += "\n";
    }
    char tail[96];
    std::snprintf(tail, sizeof(tail), "DATA sha256=%s bytes=%zu\n",
                  data_hash.final_hex().c_str(), unit);
    *out += tail;
    if (cards.empty()) warnings->push_back("HDU" + std::to_string(hdu) + ": no retained cards");
    pos = off + unit;
    ++hdu;
  }
  return hdu > 0;
}

}  // namespace

CanonicalHashResult canonical_product_hash_file(const std::string& u8path) {
  CanonicalHashResult r;
  int is_dir = 0;
  if (!aio_atomic::path_size(u8path, nullptr, &is_dir) || is_dir) {
    r.error = "not a regular file: " + u8path;
    return r;
  }
  const std::string name = file_name(u8path);
  std::string lower_name = name;
  for (char& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const bool is_json = lower_name.size() > 5 &&
                       lower_name.compare(lower_name.size() - 5, 5, ".json") == 0;
  const bool is_props_name = name == "properties";
  const std::string kSimple = "SIMPLE  ", kXtension = "XTENSION";

  // PERF-P2 S2: 头 8 字节探测（FITS 判定只需前 8 字节）—— 使 raw 分支可在
  // **不把整文件读入 std::string** 的前提下判定格式。
  std::string head;
  {
    if (!aio_file::read_head(u8path.c_str(), 8, &head)) {
      r.error = "cannot read: " + u8path;
      return r;
    }
  }

  // raw 分支（无格式特化产品, 如 *.bin）**单遍流式**: 完整性 sha256 与规范
  // sha256（= sha256(kDomain + "RAW\n" + file bytes)）在同一读遍上推进两个 SHA
  // 状态, 峰值内存从 O(文件大小) 降为 O(64KiB)。仅当「头非 FITS ∧ 非 .json ∧
  // 文件名非 properties」时进入 —— 与旧分支判定逐字节同值（properties 仍需
  // bytes.find('=') 判定, 故一律走下方整读路径）。
  if (head != kSimple && head != kXtension && !is_json && !is_props_name) {
    astrocs::crypto::Sha256 h_int, h_can;
    h_can.update(kDomain, sizeof(kDomain) - 1);
    static const char kRawTag[] = "RAW\n";
    h_can.update(kRawTag, sizeof(kRawTag) - 1);
    // CLEAN-403: 单遍流式读取经 aio (aio_file::read_stream, 峰值 = 64 KiB)。
    const bool stream_ok = aio_file::read_stream(
        u8path.c_str(),
        [&](const char* data, std::size_t n) {
          h_int.update(data, n);
          h_can.update(data, n);
          return true;
        },
        nullptr);
    if (!stream_ok) { r.error = "cannot read: " + u8path; return r; }
    r.integrity_sha256 = h_int.final_hex();
    r.format = "raw";
    r.canonical_sha256 = h_can.final_hex();
    r.warnings.push_back("no format-specific canonicalization for this file type; "
                         "canonical hash falls back to raw bytes");
    r.ok = true;
    return r;
  }

  // P3-STREAM-01：FITS 分支**不整读文件**（头部块 + 数据单元分块），峰值内存与
  // 产品大小无关；JSON / properties / raw 分支保持整读（体量小，且 raw 已在
  // 上方单遍流式分支处理）。
  if (head == kSimple || head == kXtension) {
    std::uint64_t total_bytes = 0;
    if (!aio_atomic::path_size(u8path, &total_bytes, nullptr)) {
      r.error = "cannot stat: " + u8path;
      return r;
    }
    {
      std::string h;
      if (!aio_file::sha256_hex(u8path.c_str(), &h)) {
        r.error = "cannot read: " + u8path;
        return r;
      }
      r.integrity_sha256 = h;
    }
    std::string body;
    std::vector<std::string> excl, warns;
    if (!parse_fits_canonical(u8path.c_str(), total_bytes, &body, &excl, &warns)) {
      r.error = "cannot parse FITS structure: " + u8path;
      return r;
    }
    r.format = "fits";
    r.canonical_sha256 = sha256_str(std::string(kDomain) + body);
    std::sort(excl.begin(), excl.end());
    excl.erase(std::unique(excl.begin(), excl.end()), excl.end());
    r.excluded_keys_hit = excl;
    r.warnings = warns;
    r.ok = true;
    return r;
  }
  bool ok = false;
  const std::string bytes = read_file_bytes(u8path, &ok);
  if (!ok) {
    r.error = "cannot read: " + u8path;
    return r;
  }
  r.integrity_sha256 = astrocs::crypto::sha256_hex(bytes.data(), bytes.size());

  const bool is_props = is_props_name && bytes.find('=') != std::string::npos;

  if (is_json) {
    nlohmann::json doc;
    try {
      doc = nlohmann::json::parse(bytes);
    } catch (const std::exception& e) {
      r.error = std::string("cannot parse JSON: ") + e.what();
      return r;
    }
    std::vector<std::string> excluded(
        kExcludedJsonKeys, kExcludedJsonKeys + sizeof(kExcludedJsonKeys) / sizeof(char*));
    std::vector<std::string> hits;
    const nlohmann::json pruned = prune_json(doc, excluded, &hits, "");
    std::string text;
    json_canonical_text(pruned, &text);
    r.format = "json";
    r.canonical_sha256 = sha256_str(std::string(kDomain) + "JSON\n" + text);
    std::sort(hits.begin(), hits.end());
    r.excluded_keys_hit = hits;
  } else if (is_props) {
    std::vector<std::pair<std::string, std::string>> kept;
    std::vector<std::string> hits;
    std::size_t b2 = 0;
    while (b2 <= bytes.size()) {
      std::size_t nl = bytes.find('\n', b2);
      const std::string line =
          trim_ws(bytes.substr(b2, nl == std::string::npos ? std::string::npos : nl - b2));
      if (nl == std::string::npos) {
        if (!line.empty() && line[0] != '#' && line.find('=') != std::string::npos) {
          const std::size_t eq = line.find('=');
          const std::string k = trim_ws(line.substr(0, eq));
          if (in_list(kExcludedPropsKeys,
                      sizeof(kExcludedPropsKeys) / sizeof(kExcludedPropsKeys[0]), k))
            hits.push_back(k);
          else kept.emplace_back(k, trim_ws(line.substr(eq + 1)));
        }
        break;
      }
      if (!line.empty() && line[0] != '#' && line.find('=') != std::string::npos) {
        const std::size_t eq = line.find('=');
        const std::string k = trim_ws(line.substr(0, eq));
        if (in_list(kExcludedPropsKeys,
                    sizeof(kExcludedPropsKeys) / sizeof(kExcludedPropsKeys[0]), k))
          hits.push_back(k);
        else kept.emplace_back(k, trim_ws(line.substr(eq + 1)));
      }
      b2 = nl + 1;
    }
    std::sort(kept.begin(), kept.end());
    std::string body;
    for (const auto& kv : kept) body += kv.first + "=" + kv.second + "\n";
    r.format = "hips-properties";
    r.canonical_sha256 = sha256_str(std::string(kDomain) + "PROPERTIES\n" + body);
    std::sort(hits.begin(), hits.end());
    r.excluded_keys_hit = hits;
  } else {
    r.format = "raw";
    r.canonical_sha256 = sha256_str(std::string(kDomain) + "RAW\n" + bytes);
    r.warnings.push_back("no format-specific canonicalization for this file type; "
                         "canonical hash falls back to raw bytes");
  }
  r.ok = true;
  return r;
}

}  // namespace astrocs::core
