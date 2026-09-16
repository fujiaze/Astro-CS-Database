/* v6_fits.cpp — 原生 FITS 写法/校验和/重开验证实现。
 *
 * 校验和算法 = FITS 4.0 §4.4.2.5 / Rob Seaman (ADASS 1994)：
 *   32-bit 1 补码累加（16-bit 字，even->hi, odd->lo，逐 720 字折叠），
 *   CHECKSUM 16 字符编码 (= 0xFFFFFFFF - sum 的 Seaman 编码)。
 *   本实现独立转写；外部真值 = astropy.io.fits verify_checksum/verify_datasum。
 */
#include "astro/aio/v6_fits.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace astrocs {
namespace aio {
namespace {

std::string trim_right(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
  return s;
}

std::string trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\0')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\0')) --b;
  return s.substr(a, b - a);
}

void fold_halves(std::uint32_t* hi, std::uint32_t* lo) {
  std::uint32_t hicarry = *hi >> 16;
  std::uint32_t locarry = *lo >> 16;
  while (hicarry | locarry) {
    *hi = (*hi & 0xFFFFu) + locarry;
    *lo = (*lo & 0xFFFFu) + hicarry;
    hicarry = *hi >> 16;
    locarry = *lo >> 16;
  }
}

}  // namespace

FitsCard FitsCard::make_real(const std::string& k, double v,
                             const std::string& c) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.12g", v);
  return FitsCard{k, Type::kReal, std::string(buf), c};
}

std::string fits_format_card(const FitsCard& card) {
  std::string out(80, ' ');
  std::string key = card.keyword;
  if (key.size() > 8) key = key.substr(0, 8);
  out.replace(0, key.size(), key);
  if (key == "END") return out;  // END + 77 blanks
  out[8] = '=';
  if (card.type == FitsCard::Type::kString) {
    std::string field = "'";
    for (char ch : card.value) {
      field.push_back(ch);
      if (ch == '\'') field.push_back('\'');  // FITS 转义
    }
    field.push_back('\'');
    if (field.size() <= 20) {
      out.replace(10, field.size(), field);
    } else {
      // 长字符串允许溢出 20 字符字段；为保证 80 列不越界，必要时截断。
      const std::size_t room = 80 - 10;
      if (field.size() > room) field = field.substr(0, room);
      out.replace(10, field.size(), field);
      return out;  // 无注释
    }
    if (!card.comment.empty()) {
      const std::string c = " / " + card.comment;
      if (30 + c.size() <= 80) out.replace(30, c.size(), c);
    }
    return out;
  }
  std::string field = card.value;
  if (field.size() > 20) field = field.substr(0, 20);
  out.replace(30 - field.size(), field.size(), field);
  if (!card.comment.empty()) {
    const std::string c = " / " + card.comment;
    if (30 + c.size() <= 80) out.replace(30, c.size(), c);
  }
  return out;
}

std::uint32_t fits_ones_complement_sum(const std::uint8_t* data,
                                       std::size_t n) {
  std::uint32_t hi = 0, lo = 0;
  std::uint64_t words = 0;
  for (std::size_t i = 0; i + 1 < n; i += 2) {
    const std::uint32_t w =
        (static_cast<std::uint32_t>(data[i]) << 8) | data[i + 1];
    if ((words & 1u) == 0u) {
      hi += w;
    } else {
      lo += w;
    }
    ++words;
    if ((words % 720u) == 0u) fold_halves(&hi, &lo);
  }
  fold_halves(&hi, &lo);
  return (hi << 16) | lo;
}

std::uint32_t fits_oc_add(std::uint32_t a, std::uint32_t b) {
  std::uint32_t hi = a >> 16, lo = a & 0xFFFFu;
  hi += b >> 16;
  lo += b & 0xFFFFu;
  fold_halves(&hi, &lo);
  return (hi << 16) | lo;
}

std::array<char, 16> fits_encode_checksum(std::uint32_t value) {
  // Seaman 编码：每字节 -> 4 字符 (基数 4 + ASCII '0'，避开标点)。
  static const unsigned char exclude[13] = {0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
                                            0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
                                            0x60};
  const std::uint32_t mask[4] = {0xff000000u, 0x00ff0000u, 0x0000ff00u,
                                 0x000000ffu};
  const int offset = 0x30;
  char asc[32];
  std::memset(asc, 0, sizeof(asc));
  for (int ii = 0; ii < 4; ++ii) {
    const int byte = static_cast<int>((value & mask[ii]) >> (24 - 8 * ii));
    const int quotient = byte / 4 + offset;
    const int remainder = byte % 4;
    int ch[4] = {quotient, quotient, quotient, quotient};
    ch[0] += remainder;
    for (int check = 1; check;) {
      check = 0;
      for (int kk = 0; kk < 13; ++kk) {
        for (int jj = 0; jj < 4; jj += 2) {
          if (static_cast<unsigned char>(ch[jj]) == exclude[kk] ||
              static_cast<unsigned char>(ch[jj + 1]) == exclude[kk]) {
            ch[jj]++;
            ch[jj + 1]--;
            check++;
          }
        }
      }
    }
    for (int jj = 0; jj < 4; ++jj) asc[4 * jj + ii] = static_cast<char>(ch[jj]);
  }
  std::array<char, 16> out{};
  for (int ii = 0; ii < 16; ++ii) out[ii] = asc[(ii + 15) % 16];
  return out;
}

bool fits_decode_checksum(const std::array<char, 16>& in,
                          std::uint32_t* value) {
  char cbuf[16];
  for (int ii = 0; ii < 16; ++ii) cbuf[ii] = static_cast<char>(in[(ii + 1) % 16] - 0x30);
  std::uint32_t hi = 0, lo = 0;
  for (int ii = 0; ii < 16; ii += 4) {
    hi += (static_cast<std::uint32_t>(static_cast<unsigned char>(cbuf[ii])) << 8) +
          static_cast<unsigned char>(cbuf[ii + 1]);
    lo += (static_cast<std::uint32_t>(static_cast<unsigned char>(cbuf[ii + 2])) << 8) +
          static_cast<unsigned char>(cbuf[ii + 3]);
  }
  fold_halves(&hi, &lo);
  if (value) *value = (hi << 16) | lo;
  return true;
}

std::size_t fits_padded_size(std::size_t n) {
  const std::size_t rem = n % 2880u;
  return rem == 0 ? n : n + (2880u - rem);
}

std::size_t fits_bytes_per_pixel(int bitpix) {
  switch (bitpix) {
    case 8: return 1;
    case 16: return 2;
    case 32: return 4;
    case -32: return 4;
    case -64: return 8;
    default: return 0;
  }
}

std::size_t fits_data_size(const FitsHduSpec& spec) {
  const std::size_t bpp = fits_bytes_per_pixel(spec.bitpix);
  if (bpp == 0 || spec.naxis.empty()) return 0;
  std::size_t total = bpp;
  for (std::size_t d : spec.naxis) total *= d;
  return total;
}

bool FitsStreamWriter::pwrite_all(std::uint64_t off, const void* data,
                                  std::size_t len, std::string* err) {
#if defined(_WIN32)
  if (_lseeki64(fd_, static_cast<long long>(off), SEEK_SET) < 0) {
    if (err) *err = "seek failed";
    return false;
  }
  const char* p = static_cast<const char*>(data);
  std::size_t left = len;
  while (left > 0) {
    const unsigned chunk = static_cast<unsigned>(left > 1u << 30 ? 1u << 30 : left);
    const int w = _write(fd_, p, chunk);
    if (w <= 0) {
      if (err) *err = "write failed";
      return false;
    }
    p += w;
    left -= static_cast<std::size_t>(w);
  }
  return true;
#else
  const char* p = static_cast<const char*>(data);
  std::size_t left = len;
  while (left > 0) {
    const ssize_t w = ::pwrite(fd_, p, left, static_cast<off_t>(off));
    if (w < 0) {
      if (errno == EINTR) continue;
      if (err) *err = std::string("pwrite failed: ") + std::strerror(errno);
      return false;
    }
    p += w;
    left -= static_cast<std::size_t>(w);
    off += static_cast<std::uint64_t>(w);
  }
  return true;
#endif
}

bool FitsStreamWriter::begin_hdu(const FitsHduSpec& spec, std::string* err) {
  if (in_hdu_) {
    if (err) *err = "begin_hdu while previous HDU open";
    return false;
  }
  if (fits_bytes_per_pixel(spec.bitpix) == 0 || spec.naxis.empty()) {
    if (err) *err = "invalid BITPIX or empty NAXIS";
    return false;
  }
  std::vector<FitsCard> cards;
  const bool primary = spec.extname.empty();
  if (primary) {
    cards.push_back(FitsCard::make_logical("SIMPLE", true, "conforms to FITS"));
    cards.push_back(FitsCard::make_integer("BITPIX", spec.bitpix, ""));
    cards.push_back(FitsCard::make_integer("NAXIS",
                                           static_cast<long long>(spec.naxis.size())));
    for (std::size_t i = 0; i < spec.naxis.size(); ++i) {
      cards.push_back(FitsCard::make_integer("NAXIS" + std::to_string(i + 1),
                                             static_cast<long long>(spec.naxis[i])));
    }
    if (spec.primary_has_extensions) {
      cards.push_back(FitsCard::make_logical("EXTEND", true,
                                             "extensions may be present"));
    }
  } else {
    cards.push_back(FitsCard::make_string("XTENSION", "IMAGE", "Image extension"));
    cards.push_back(FitsCard::make_integer("BITPIX", spec.bitpix, ""));
    cards.push_back(FitsCard::make_integer("NAXIS",
                                           static_cast<long long>(spec.naxis.size())));
    for (std::size_t i = 0; i < spec.naxis.size(); ++i) {
      cards.push_back(FitsCard::make_integer("NAXIS" + std::to_string(i + 1),
                                             static_cast<long long>(spec.naxis[i])));
    }
    if (spec.with_pcount_gcount) {
      cards.push_back(FitsCard::make_integer("PCOUNT", 0, "no random parameters"));
      cards.push_back(FitsCard::make_integer("GCOUNT", 1, "one data group"));
    }
    cards.push_back(FitsCard::make_string("EXTNAME", spec.extname, "HDU name"));
  }
  for (const auto& c : spec.cards) cards.push_back(c);
  // 占位 DATASUM / CHECKSUM（位于 END 之前最后两个关键字）。
  cards.push_back(FitsCard::make_string("DATASUM", "0",
                                        "data unit checksum updated"));
  const std::size_t datasum_idx = cards.size() - 1;
  cards.push_back(FitsCard::make_string("CHECKSUM", "0000000000000000",
                                        "HDU checksum updated"));
  const std::size_t checksum_idx = cards.size() - 1;
  cards.push_back(FitsCard::make_string("END", "", ""));

  header_.clear();
  for (const auto& c : cards) {
    const std::string s = fits_format_card(c);
    header_.insert(header_.end(), s.begin(), s.end());
  }
  header_.resize(fits_padded_size(header_.size()), static_cast<std::uint8_t>(' '));

  hdu_start_ = file_pos_;
  datasum_card_off_ = datasum_idx * 80;
  checksum_card_off_ = checksum_idx * 80;
  if (!pwrite_all(hdu_start_, header_.data(), header_.size(), err)) return false;
  file_pos_ += header_.size();

  data_size_ = fits_data_size(spec);
  data_written_ = 0;
  acc_hi_ = acc_lo_ = 0;
  acc_words_ = 0;
  acc_pending_ = -1;
  in_hdu_ = true;
  return true;
}

bool FitsStreamWriter::write_raw(const void* data, std::size_t len,
                                 std::string* err) {
  // 追加到 1 补码累加器（跨块保持 hi/lo 奇偶一致）。
  const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
  std::size_t i = 0;
  if (acc_pending_ >= 0 && len > 0) {
    const std::uint32_t w =
        (static_cast<std::uint32_t>(acc_pending_) << 8) | p[0];
    if ((acc_words_ & 1u) == 0u) acc_hi_ += w; else acc_lo_ += w;
    ++acc_words_;
    if ((acc_words_ % 720u) == 0u) fold_halves(&acc_hi_, &acc_lo_);
    acc_pending_ = -1;
    i = 1;
  }
  for (; i + 1 < len; i += 2) {
    const std::uint32_t w =
        (static_cast<std::uint32_t>(p[i]) << 8) | p[i + 1];
    if ((acc_words_ & 1u) == 0u) acc_hi_ += w; else acc_lo_ += w;
    ++acc_words_;
    if ((acc_words_ % 720u) == 0u) fold_halves(&acc_hi_, &acc_lo_);
  }
  if (i < len) acc_pending_ = p[i];

  if (!pwrite_all(file_pos_, data, len, err)) return false;
  file_pos_ += len;
  return true;
}

bool FitsStreamWriter::write_data(const void* data, std::size_t len,
                                  std::string* err) {
  if (!in_hdu_) {
    if (err) *err = "write_data without open HDU";
    return false;
  }
  if (data_written_ + len > data_size_) {
    if (err) *err = "write_data exceeds HDU data size";
    return false;
  }
  if (!write_raw(data, len, err)) return false;
  data_written_ += len;
  return true;
}

bool FitsStreamWriter::end_hdu(std::string* err) {
  if (!in_hdu_) {
    if (err) *err = "end_hdu without open HDU";
    return false;
  }
  // 数据段补零到 2880 对齐（零字不改变 1 补码和）。
  const std::size_t padded = fits_padded_size(data_size_);
  if (data_written_ < padded) {
    const std::size_t pad = padded - data_written_;
    std::vector<std::uint8_t> zeros(pad, 0);
    std::string werr;
    if (!write_raw(zeros.data(), zeros.size(), &werr)) {
      if (err) *err = werr;
      return false;
    }
  }
  // 折叠并得出 DATASUM。
  std::uint32_t hi = acc_hi_, lo = acc_lo_;
  fold_halves(&hi, &lo);
  const std::uint32_t datasum = (hi << 16) | lo;
  std::string card = fits_format_card(FitsCard::make_string(
      "DATASUM", std::to_string(datasum), "data unit checksum updated"));
  std::memset(header_.data() + datasum_card_off_, ' ', 80);
  std::memcpy(header_.data() + datasum_card_off_, card.data(), 80);

  // 头校验和（此时 CHECKSUM 仍为占位 16 个 '0'）。
  const std::uint32_t header_sum =
      fits_ones_complement_sum(header_.data(), header_.size());
  const std::uint32_t total = fits_oc_add(datasum, header_sum);
  const std::array<char, 16> enc =
      fits_encode_checksum(0xFFFFFFFFu - total);
  std::string ckc = fits_format_card(FitsCard::make_string(
      "CHECKSUM", std::string(enc.data(), enc.size()), "HDU checksum updated"));
  std::memset(header_.data() + checksum_card_off_, ' ', 80);
  std::memcpy(header_.data() + checksum_card_off_, ckc.data(), 80);

  // 自检：完整 HDU（含编码 CHECKSUM）的 1 补码和必须为 0 或 0xFFFFFFFF。
  const std::uint32_t verify_hs =
      fits_ones_complement_sum(header_.data(), header_.size());
  const std::uint32_t combined = fits_oc_add(datasum, verify_hs);
  if (combined != 0u && combined != 0xFFFFFFFFu) {
    if (err) *err = "internal checksum self-check failed";
    return false;
  }
  if (!pwrite_all(hdu_start_, header_.data(), header_.size(), err)) return false;

  last_datasum_ = datasum;
  last_checksum_.assign(enc.data(), enc.size());
  last_datasum_text_ = std::to_string(datasum);
  in_hdu_ = false;
  return true;
}

bool FitsStreamWriter::fsync_now(std::string* err) {
#if defined(_WIN32)
  if (_commit(fd_) != 0) {
    if (err) *err = "commit failed";
    return false;
  }
  return true;
#else
  if (::fsync(fd_) != 0) {
    if (err) *err = std::string("fsync failed: ") + std::strerror(errno);
    return false;
  }
  return true;
#endif
}

// ── 重开验证 ──────────────────────────────────────────────────────────────
namespace {

struct ParsedCards {
  std::map<std::string, std::string> strings;
  std::map<std::string, long long> integers;
  std::map<std::string, double> reals;
  std::map<std::string, bool> logicals;
};

// 返回 false 表示遇到 END。
bool parse_one_card(const std::uint8_t* card, ParsedCards* out,
                    std::string* keyword) {
  std::string key(reinterpret_cast<const char*>(card), 8);
  key = trim(key);
  *keyword = key;
  if (key == "END") return false;
  if (card[8] != '=') return true;  // 纯注释卡
  std::string field(reinterpret_cast<const char*>(card + 10), 70);
  std::size_t i = 0;
  while (i < field.size() && field[i] == ' ') ++i;
  if (i < field.size() && field[i] == '\'') {
    std::string v;
    ++i;
    while (i < field.size()) {
      if (field[i] == '\'') {
        if (i + 1 < field.size() && field[i + 1] == '\'') {
          v.push_back('\'');
          i += 2;
          continue;
        }
        break;
      }
      v.push_back(field[i]);
      ++i;
    }
    (*out).strings[key] = trim_right(v);
    return true;
  }
  // 非字符串值只占列 11..30（0-based 字段下标 0..19），其后为注释。
  std::string t = field.substr(0, 20);
  {
    std::size_t a = 0;
    while (a < t.size() && t[a] == ' ') ++a;
    t = trim_right(t.substr(a));
  }
  if (t.empty()) return true;
  if (t == "T" || t == "F") {
    (*out).logicals[key] = (t == "T");
    return true;
  }
  char* end = nullptr;
  const long long lv = std::strtoll(t.c_str(), &end, 10);
  if (end != nullptr && *end == '\0') {
    (*out).integers[key] = lv;
    return true;
  }
  end = nullptr;
  const double dv = std::strtod(t.c_str(), &end);
  if (end != nullptr && *end == '\0') {
    (*out).reals[key] = dv;
    return true;
  }
  (*out).strings[key] = t;
  return true;
}

std::vector<std::size_t> read_naxis(const ParsedCards& pc, bool* ok) {
  std::vector<std::size_t> n;
  auto it = pc.integers.find("NAXIS");
  if (it == pc.integers.end() || it->second < 1 || it->second > 999) {
    *ok = false;
    return n;
  }
  for (long long i = 1; i <= it->second; ++i) {
    auto d = pc.integers.find("NAXIS" + std::to_string(i));
    if (d == pc.integers.end() || d->second < 0) {
      *ok = false;
      return n;
    }
    n.push_back(static_cast<std::size_t>(d->second));
  }
  *ok = true;
  return n;
}

std::string join_naxis(const std::vector<std::size_t>& n) {
  std::string s;
  for (std::size_t v : n) {
    if (!s.empty()) s += ",";
    s += std::to_string(v);
  }
  return s;
}

FitsVerifyResult verify_impl(const std::uint8_t* data, std::size_t size,
                             const std::vector<ExpectedHdu>& expected) {
  FitsVerifyResult res;
  std::size_t pos = 0;
  std::size_t hdu_index = 0;
  bool first = true;
  while (pos < size) {
    if (pos + 2880 > size) {
      res.violations.push_back({"G-FITS-HEADER", "ALG-P3-008",
                                "truncated header at HDU " + std::to_string(hdu_index)});
      break;
    }
    // 读 2880 块直到 END。
    std::size_t header_blocks = 0;
    bool found_end = false;
    ParsedCards pc;
    for (std::size_t b = 0; pos + (b + 1) * 2880 <= size; ++b) {
      const std::uint8_t* blk = data + pos + b * 2880;
      bool stop = false;
      for (int c = 0; c < 36; ++c) {
        std::string kw;
        if (!parse_one_card(blk + c * 80, &pc, &kw)) {
          stop = true;
          break;
        }
      }
      if (stop) {
        header_blocks = b + 1;
        found_end = true;
        break;
      }
    }
    if (!found_end) {
      res.violations.push_back({"G-FITS-HEADER", "ALG-P3-008",
                                "no END card at HDU " + std::to_string(hdu_index)});
      break;
    }
    const std::uint64_t header_bytes = header_blocks * 2880;

    FitsHduInfo info;
    info.header_bytes = header_bytes;
    info.is_primary = first;
    auto bitpix_it = pc.integers.find("BITPIX");
    if (bitpix_it == pc.integers.end()) {
      res.violations.push_back({"G-FITS-HEADER", "ALG-P3-008",
                                "missing BITPIX at HDU " + std::to_string(hdu_index)});
      break;
    }
    info.bitpix = static_cast<int>(bitpix_it->second);
    bool naxis_ok = false;
    info.naxis = read_naxis(pc, &naxis_ok);
    if (!naxis_ok) {
      res.violations.push_back({"G-FITS-HEADER", "ALG-P3-008",
                                "invalid NAXIS at HDU " + std::to_string(hdu_index)});
      break;
    }
    auto bunit_it = pc.strings.find("BUNIT");
    if (bunit_it != pc.strings.end()) info.bunit = bunit_it->second;
    if (first) {
      auto s = pc.logicals.find("SIMPLE");
      if (s == pc.logicals.end() || !s->second) {
        res.violations.push_back({"G-FITS-HEADER", "ALG-P3-008",
                                  "PRIMARY missing SIMPLE=T"});
      }
      info.extname.clear();
    } else {
      auto x = pc.strings.find("XTENSION");
      if (x == pc.strings.end() || x->second != "IMAGE") {
        res.violations.push_back({"G-FITS-HEADER", "ALG-P3-008",
                                  "extension missing XTENSION='IMAGE'"});
      }
      auto e = pc.strings.find("EXTNAME");
      info.extname = (e != pc.strings.end()) ? e->second : std::string();
    }

    // 数据段。
    const std::size_t bpp = fits_bytes_per_pixel(info.bitpix);
    std::size_t raw = bpp;
    for (std::size_t d : info.naxis) raw *= d;
    const std::size_t padded = fits_padded_size(raw);
    const std::uint64_t data_off = static_cast<std::uint64_t>(pos) + header_bytes;
    if (data_off + padded > size) {
      res.violations.push_back({"G-FITS-TRUNCATED", "ALG-P3-008",
                                "data segment truncated at HDU " + std::to_string(hdu_index)});
      break;
    }
    info.data_bytes = padded;

    // DATASUM。
    auto ds = pc.strings.find("DATASUM");
    if (ds == pc.strings.end()) {
      res.violations.push_back({"G-FITS-DATASUM", "ALG-P3-008",
                                "missing DATASUM at HDU " + std::to_string(hdu_index)});
    } else {
      const std::uint32_t want = static_cast<std::uint32_t>(std::strtoull(ds->second.c_str(), nullptr, 10));
      const std::uint32_t got = fits_ones_complement_sum(data + data_off, padded);
      info.datasum = got;
      if (want != got) {
        res.violations.push_back({"G-FITS-DATASUM", "ALG-P3-008",
                                  "DATASUM mismatch at HDU " + std::to_string(hdu_index) +
                                      ": header=" + ds->second + " recomputed=" + std::to_string(got)});
      }
    }

    // CHECKSUM：完整 HDU 1 补码和必须为 0 或 0xFFFFFFFF。
    auto ck = pc.strings.find("CHECKSUM");
    if (ck == pc.strings.end() || ck->second.size() != 16) {
      res.violations.push_back({"G-FITS-CHECKSUM", "ALG-P3-008",
                                "missing/invalid CHECKSUM at HDU " + std::to_string(hdu_index)});
    } else {
      info.checksum = ck->second;
      const std::uint32_t hs = fits_ones_complement_sum(data + pos, header_bytes);
      const std::uint32_t combined = fits_oc_add(info.datasum, hs);
      if (combined != 0u && combined != 0xFFFFFFFFu) {
        res.violations.push_back({"G-FITS-CHECKSUM", "ALG-P3-008",
                                  "CHECKSUM verification failed at HDU " + std::to_string(hdu_index)});
      }
    }

    // 期望对照。
    if (hdu_index < expected.size()) {
      const ExpectedHdu& ex = expected[hdu_index];
      if (ex.extname != info.extname) {
        res.violations.push_back({"G-FITS-LAYER", "ALG-P3-008",
                                  "HDU " + std::to_string(hdu_index) + " extname '" + info.extname +
                                      "' != expected '" + ex.extname + "'"});
      }
      if (ex.bitpix != 0 && ex.bitpix != info.bitpix) {
        res.violations.push_back({"G-FITS-LAYER", "ALG-P3-008",
                                  "HDU " + std::to_string(hdu_index) + " BITPIX mismatch"});
      }
      if (!ex.naxis.empty() && ex.naxis != info.naxis) {
        res.violations.push_back({"G-FITS-LAYER", "ALG-P3-008",
                                  "HDU " + std::to_string(hdu_index) + " shape [" + join_naxis(info.naxis) +
                                      "] != expected [" + join_naxis(ex.naxis) + "]"});
      }
      if (ex.check_bunit && ex.bunit != info.bunit) {
        res.violations.push_back({"G-FITS-BUNIT", "FZ-BUNIT-SEMANTICS",
                                  "HDU " + std::to_string(hdu_index) + " BUNIT '" + info.bunit +
                                      "' != expected '" + ex.bunit + "'"});
      }
      for (const auto& kv : ex.string_cards) {
        auto it = pc.strings.find(kv.first);
        if (it == pc.strings.end() || it->second != kv.second) {
          res.violations.push_back({"G-FITS-KEYWORD", "ALG-P3-008",
                                    "HDU " + std::to_string(hdu_index) + " keyword " + kv.first +
                                        " mismatch"});
        }
      }
    }

    res.hdus.push_back(info);
    pos = static_cast<std::size_t>(data_off + padded);
    ++hdu_index;
    first = false;
  }
  if (!expected.empty() && res.hdus.size() != expected.size()) {
    res.violations.push_back({"G-FITS-LAYER", "ALG-P3-008",
                              "HDU count " + std::to_string(res.hdus.size()) + " != expected " +
                                  std::to_string(expected.size())});
  }
  res.ok = res.violations.empty();
  return res;
}

}  // namespace

FitsVerifyResult verify_fits_bytes(const std::vector<std::uint8_t>& bytes,
                                   const std::vector<ExpectedHdu>& expected) {
  return verify_impl(bytes.data(), bytes.size(), expected);
}

FitsVerifyResult verify_fits_file(const std::string& path,
                                  const std::vector<ExpectedHdu>& expected) {
  FitsVerifyResult res;
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    res.error = "cannot open file: " + path;
    return res;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (in.bad()) {
    res.error = "read error: " + path;
    return res;
  }
  return verify_impl(bytes.data(), bytes.size(), expected);
}

}  // namespace aio
}  // namespace astrocs
