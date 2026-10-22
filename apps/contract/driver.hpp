#pragma once
// The contract driver: it decodes one request envelope, calls exactly one
// public API entry point and encodes the response.  It performs no scientific
// computation and never relaxes a tolerance to match an expected result.
#include <filesystem>
#include <string>

#include "json_io.hpp"

namespace acsd::contract {

// Runs the request stored in `request_path` and writes the response document
// to `response_path`.  Returns the process exit code: 0 for a handled request
// (including a scientific nonconvergence and an unsupported operation) and 2
// for a syntax, decoding or file reading failure.
int run_contract_file(const std::filesystem::path& request_path,
                      const std::filesystem::path& response_path, std::string& error);

// Same, for an already parsed envelope.  Used by the tests and by the CLI.
int run_contract(const json& envelope, const std::filesystem::path& response_path,
                 const std::filesystem::path& base_directory, std::string& error);

// The operation names that the driver implements.  The list is a subset of the
// free functions declared in include/acsd/contracts.hpp.
bool operation_supported(const std::string& operation);

}  // namespace acsd::contract
