// CORE-001 单元测试: Result/Error/Cancel 语义
#include "astrocs/core/contracts.h"

#include <cassert>
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

static void test_error_domains() {
  CHECK(error_domain_name(ErrorDomain::CONFIG) == std::string("CONFIG"));
  CHECK(error_domain_name(ErrorDomain::CANCELLED) == std::string("CANCELLED"));
  CHECK(error_domain_name(ErrorDomain::INTERNAL) == std::string("INTERNAL"));
}

static void test_result_ok() {
  Result<int> r = Result<int>::ok(42);
  CHECK(r.ok());
  CHECK(!r.failed());
  CHECK(r.value() == 42);
}

static void test_result_error() {
  Result<int> r = Result<int>::fail(Error(ErrorDomain::DATA, "bad schema"));
  CHECK(r.failed());
  CHECK(r.error().domain() == ErrorDomain::DATA);
  CHECK(r.error().message() == "bad schema");
  bool threw = false;
  try { (void)r.value(); } catch (const std::logic_error&) { threw = true; }
  CHECK(threw);
}

static void test_result_void() {
  Result<void> r = Result<void>::success();
  CHECK(r.ok());
  Result<void> e = Result<void>::fail(Error(ErrorDomain::IO, "disk"));
  CHECK(e.failed());
  CHECK(e.error().domain() == ErrorDomain::IO);
}

static void test_nested_cause() {
  Error inner(ErrorDomain::IO, "read failed");
  Error outer(ErrorDomain::BACKEND, "kernel failed");
  outer.set_cause(inner);
  CHECK(outer.has_cause());
  CHECK(outer.cause()->domain() == ErrorDomain::IO);
  auto chain = outer.cause_chain();
  CHECK(chain.size() == 2);
  CHECK(chain[0].find("BACKEND") != std::string::npos);
  CHECK(chain[1].find("IO") != std::string::npos);
}

static void test_cancel() {
  CancellationToken tok;
  CHECK(!tok.cancelled());
  tok.cancel();
  CHECK(tok.cancelled());
  tok.reset();
  CHECK(!tok.cancelled());
}

static void test_value_or() {
  Result<int> ok = Result<int>::ok(7);
  CHECK(ok.value_or(99) == 7);
  Result<int> err = Result<int>::fail(Error(ErrorDomain::CONFIG, "bad"));
  CHECK(err.value_or(99) == 99);
}

int main() {
  test_error_domains();
  test_result_ok();
  test_result_error();
  test_result_void();
  test_nested_cause();
  test_cancel();
  test_value_or();
  if (failures == 0) {
    std::printf("CORE-001 TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "CORE-001 TESTS FAIL (%d)\n", failures);
  return 1;
}
