// CORE-008 单元测试: 统一日志 + 指标
#include "astrocs/core/logging.h"

#include <cstdio>
#include <string>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void test_logger_emit() {
  Logger logger(LogLevel::INFO);
  auto sink = std::make_shared<InMemorySink>();
  logger.attach(sink);
  logger.info("phase1", "started");
  logger.warn("phase1", "low snr");
  logger.error("phase1", "boom");
  CHECK(sink->size() == 3);
  CHECK(logger.emitted() == 3);
  CHECK(sink->events()[0].level == LogLevel::INFO);
  CHECK(sink->events()[1].level == LogLevel::WARN);
  CHECK(sink->events()[2].level == LogLevel::ERROR);
}

static void test_logger_filter() {
  Logger logger(LogLevel::WARN);  // DEBUG/INFO 被过滤
  auto sink = std::make_shared<InMemorySink>();
  logger.attach(sink);
  logger.info("x", "hidden");
  logger.warn("x", "visible");
  CHECK(sink->size() == 1);
  CHECK(sink->events()[0].level == LogLevel::WARN);
}

static void test_jsonl_format() {
  Logger logger(LogLevel::DEBUG);
  auto sink = std::make_shared<InMemorySink>();
  logger.attach(sink);
  logger.log(LogLevel::INFO, "phase2", "progress", "50% done",
             "node-3", "run-9", 0.5, 1234);
  CHECK(sink->size() == 1);
  std::string line = sink->events()[0].to_jsonl();
  CHECK(line.find("\"component\":\"phase2\"") != std::string::npos);
  CHECK(line.find("\"node_id\":\"node-3\"") != std::string::npos);
  CHECK(line.find("\"run_id\":\"run-9\"") != std::string::npos);
  CHECK(line.find("\"progress\":0.500") != std::string::npos);
  CHECK(line.find("\"wall_us\":1234") != std::string::npos);
  CHECK(line.find("\"seq\":1") != std::string::npos);
}

static void test_seq_increment() {
  Logger logger(LogLevel::INFO);
  auto sink = std::make_shared<InMemorySink>();
  logger.attach(sink);
  logger.info("a", "1");
  logger.info("a", "2");
  logger.info("a", "3");
  CHECK(sink->events()[0].seq == 1);
  CHECK(sink->events()[2].seq == 3);
}

static void test_metrics_aggregate() {
  MetricsAggregator agg;
  agg.record("wall_us", 100);
  agg.record("wall_us", 200);
  agg.record("wall_us", 50);
  agg.record("bytes", 1000);
  CHECK(agg.count("wall_us") == 3);
  CHECK(agg.sum("wall_us") == 350);
  CHECK(agg.max("wall_us") == 200);
  CHECK(agg.names().size() == 2);
  std::string jsonl = agg.export_jsonl();
  CHECK(jsonl.find("\"metric\":\"wall_us\"") != std::string::npos);
  CHECK(jsonl.find("\"sum\":350") != std::string::npos);
}

int main() {
  test_logger_emit();
  test_logger_filter();
  test_jsonl_format();
  test_seq_increment();
  test_metrics_aggregate();
  if (failures == 0) {
    std::printf("CORE-008 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-008 TESTS FAIL (%d)\n", failures);
  return 1;
}
