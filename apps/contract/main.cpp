// The acsd-contract entry point: it reads a request envelope, runs one
// public API call and writes the response document.
#include <filesystem>
#include <iostream>
#include <string>

#include "driver.hpp"

namespace {

void print_usage() {
  std::cout << "usage: acsd-contract --request <request.json> --response <response.json>\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path request_path;
  std::filesystem::path response_path;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--request" && i + 1 < argc) {
      request_path = argv[++i];
    } else if (argument == "--response" && i + 1 < argc) {
      response_path = argv[++i];
    } else if (argument == "--help" || argument == "-h") {
      print_usage();
      return 0;
    } else {
      std::cerr << "acsd-contract: unknown argument " << argument << "\n";
      print_usage();
      return 2;
    }
  }
  if (request_path.empty() || response_path.empty()) {
    std::cerr << "acsd-contract: both --request and --response are required\n";
    print_usage();
    return 2;
  }

  std::string error;
  const int code = acsd::contract::run_contract_file(request_path, response_path, error);
  if (code != 0) {
    std::cerr << "acsd-contract: " << error << "\n";
  }
  return code;
}
