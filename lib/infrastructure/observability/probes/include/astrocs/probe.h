// lib/infrastructure/observability/probes/include/astrocs/probe.h
// ============================================================================
// RELEASE-02 性能探针框架 —— 单一开关、两层控制
// ============================================================================
//
// 设计意图（负责人原话）:
//   「用一个和程序同步启动的 Python 做系统性能利用率统计，然后再程序内部插入
//    一些探针。这些探针函数共用一个开关。且最好对性能只有微量影响。
//    这样不用的时候还可以把这些探针直接在 cmake 里面关掉。」
//
// 两层开关:
//   1) 编译期  ASTROCS_PROBES (CMake option, 默认 OFF)
//        OFF -> 本头文件只定义空语句宏 ((void)0): 无符号、无计时、无分配、
//               不编译 probe.cpp, 链接产物中不存在任何 astrocs::probe::* 符号。
//        ON  -> 探针实现参与编译, 宏展开为真实的 RAII 计时/计数调用。
//   2) 运行期  ASTROCS_PROBE_LOG=<path> (环境变量)
//        未设置/空 -> 即便编译期 ON 也不产出任何文件、不做计时(仅一次 env 读取)。
//        已设置   -> JSONL 写入 <path> (追加模式)。
//
// 可选调优环境变量:
//   ASTROCS_PROBE_FLUSH_LINES=<n>  行数阈值 (默认 1024)
//   ASTROCS_PROBE_FLUSH_MS=<n>     时间阈值毫秒 (默认 1000)
//
// 输出格式: JSONL, 每行一个对象, 至少含
//   ts_utc (RFC3339 UTC 毫秒), thread_id, scope, name, wall_us
// 计数探针追加 count; 取值探针追加 last/min/max/n; 作用域标签作为顶层字段。
//
// 热路径约束:
//   - 计时统一 std::chrono::steady_clock;
//   - 线程本地缓冲 + 线程本地计数/取值聚合, 按行数或时间批量 flush;
//   - 仅 flush 点持有一把互斥锁写文件, 记录路径无锁;
//   - 线程退出时自动 flush; 主进程 atexit 再兜底 flush。
//
// 线程安全: 任意线程可调用; 不做线程数/ISA/block 假设 (AGENTS §5)。
// 科学中立: 探针只观测, 不改变任何数值/控制流 (除显式 flush 点)。
// ============================================================================
#pragma once

#if defined(ASTROCS_PROBES) && (ASTROCS_PROBES)

#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <type_traits>

namespace astrocs {
namespace probe {

// 运行期开关: 仅当 ASTROCS_PROBE_LOG 指向非空路径时为 true。
// 实现内部缓存, 反复调用代价 = 一次函数局部静态读取。
bool enabled() noexcept;

// 返回运行期输出路径; 未启用返回 nullptr。供实现/测试使用。
const char* log_path() noexcept;

// RAII 作用域计时器。
//
// 两参数构造: (scope, name) 直接给出两个字段。
// 一参数构造: ("phase1.drizzle.frame") 在第一个 '.' 处切分
//             -> scope="phase1", name="drizzle.frame"; 无 '.' 时 scope=name。
//
// 未启用 (enabled()==false) 时不调用 steady_clock::now(), 析构立即返回。
class ScopeTimer {
 public:
  ScopeTimer(const char* scope, const char* name) noexcept;
  explicit ScopeTimer(const char* qualified_name) noexcept;
  ~ScopeTimer();
  ScopeTimer(const ScopeTimer&) = delete;
  ScopeTimer& operator=(const ScopeTimer&) = delete;

  // 追加 key=value 上下文标签 (最多 8 条, 超出丢弃并在记录里标记)。
  // 字符串标签按借用指针保存: 其生命周期必须覆盖本作用域计时器。
  // 算术类型走单一模板, 避免 int/long/size_t 在多候选下二义。
  template <typename T,
            typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  ScopeTimer& tag(const char* key, T value) noexcept {
    if (!active_ || n_tags_ >= kMaxTags) {
      if (n_tags_ >= kMaxTags) tags_dropped_ = true;
      return *this;
    }
    Tag& tg = tags_[n_tags_++];
    tg.key = key;
    if constexpr (std::is_floating_point<T>::value) {
      tg.kind = TagKind::kDouble;
      tg.d = static_cast<double>(value);
    } else if constexpr (std::is_signed<T>::value) {
      tg.kind = TagKind::kInt;
      tg.i = static_cast<long long>(value);
    } else {
      tg.kind = TagKind::kUint;
      tg.u = static_cast<unsigned long long>(value);
    }
    return *this;
  }
  ScopeTimer& tag(const char* key, const char* value) noexcept;

  // 提前结束并落记录 (供 ASTROCS_PROBE_SCOPE_END 使用); 幂等, 之后析构不再重复。
  void stop() noexcept;

 private:
  enum class TagKind : unsigned char { kInt, kUint, kDouble, kString };
  struct Tag {
    const char* key = nullptr;
    TagKind kind = TagKind::kInt;
    long long i = 0;
    unsigned long long u = 0;
    double d = 0.0;
    std::string s;  // 字符串标签按值保存, 不受调用方字符串生命周期影响
  };
  static constexpr std::size_t kMaxTags = 8;

  const char* scope_ = nullptr;
  const char* name_ = nullptr;
  std::size_t scope_len_ = 0;  // 0 = 以 NUL 结尾; 否则为一参数构造切分出的前缀长度
  std::chrono::steady_clock::time_point start_{};
  bool active_ = false;
  std::size_t n_tags_ = 0;
  bool tags_dropped_ = false;
  std::array<Tag, kMaxTags> tags_{};
};

// 计数探针: 线程本地累加, flush 时每个 (scope,name) 输出一行, 含 count。
// delta 允许为负。
void count_add(const char* scope, const char* name, long long delta) noexcept;

// 取值探针: 线程本地聚合 last/min/max/n, flush 时每个 (scope,name) 输出一行。
void gauge_set(const char* scope, const char* name, double value) noexcept;

// 手动 flush 当前线程缓冲 (测试/收尾用)。
void flush() noexcept;

}  // namespace probe
}  // namespace astrocs

// ── 宏 ───────────────────────────────────────────────────────────────────────
#define ASTROCS_PROBE_ENABLED() (::astrocs::probe::enabled())

#define ASTROCS_PROBE_DETAIL_CAT_(a, b) a##b
#define ASTROCS_PROBE_DETAIL_CAT(a, b) ASTROCS_PROBE_DETAIL_CAT_(a, b)

#define ASTROCS_PROBE_DETAIL_SCOPE1(name)                     \
  ::astrocs::probe::ScopeTimer ASTROCS_PROBE_DETAIL_CAT(      \
      _astrocs_probe_scope_, __COUNTER__)(name)
#define ASTROCS_PROBE_DETAIL_SCOPE2(scope, name)              \
  ::astrocs::probe::ScopeTimer ASTROCS_PROBE_DETAIL_CAT(      \
      _astrocs_probe_scope_, __COUNTER__)(scope, name)
#define ASTROCS_PROBE_DETAIL_PICK(_1, _2, NAME, ...) NAME
#define ASTROCS_PROBE_DETAIL_UNUSED(...) ((void)0)

// ASTROCS_PROBE_SCOPE("phase1.drizzle.frame")
// ASTROCS_PROBE_SCOPE("phase1.drizzle", "frame")
#define ASTROCS_PROBE_SCOPE(...)                                              \
  ASTROCS_PROBE_DETAIL_PICK(__VA_ARGS__, ASTROCS_PROBE_DETAIL_SCOPE2,         \
                            ASTROCS_PROBE_DETAIL_SCOPE1,                      \
                            ASTROCS_PROBE_DETAIL_UNUSED)(__VA_ARGS__)

// 命名作用域 (需要后续 ASTROCS_PROBE_TAG 时使用):
//   ASTROCS_PROBE_SCOPE_CTX(t, "phase1", "calibrate.frame");
//   ASTROCS_PROBE_TAG(t, "frame_key", key.c_str());
#define ASTROCS_PROBE_SCOPE_CTX(var, scope, name) \
  ::astrocs::probe::ScopeTimer var(scope, name)
#define ASTROCS_PROBE_SCOPE_END(var) ((var).stop())
#define ASTROCS_PROBE_TAG(var, key, value) ((var).tag((key), (value)))

#define ASTROCS_PROBE_COUNT(scope, name, delta) \
  ::astrocs::probe::count_add((scope), (name), (delta))
#define ASTROCS_PROBE_GAUGE(scope, name, value) \
  ::astrocs::probe::gauge_set((scope), (name), (value))
#define ASTROCS_PROBE_FLUSH() (::astrocs::probe::flush())

#else  // ASTROCS_PROBES off —— 零开销空语句, 不引入任何符号/头文件依赖

#define ASTROCS_PROBE_ENABLED() (false)
#define ASTROCS_PROBE_SCOPE(...) ((void)0)
#define ASTROCS_PROBE_SCOPE_CTX(var, scope, name) ((void)0)
#define ASTROCS_PROBE_SCOPE_END(var) ((void)0)
#define ASTROCS_PROBE_TAG(var, key, value) ((void)0)
#define ASTROCS_PROBE_COUNT(scope, name, delta) ((void)0)
#define ASTROCS_PROBE_GAUGE(scope, name, value) ((void)0)
#define ASTROCS_PROBE_FLUSH() ((void)0)

#endif  // ASTROCS_PROBES
