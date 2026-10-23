// Minimal manifest exporter: build the production registry assembly, freeze it
// and write manifest_json() atomically.
//
// This executable exists so the build product build/process-manifest.json can
// be regenerated without a second handwritten registry.  It never invents a
// process: it calls the generated assembly entry points and reports whatever
// the linked modules registered.
#include <acsd/runtime/generated_manifest.hpp>
#include <acsd/runtime/process.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

using acsd::Code;
namespace generated = acsd::generated;

#if !defined(ACSD_HAS_GENERATED_ASSEMBLY)
namespace acsd::generated {

// Weak fallbacks keep the exporter linkable before the build has generated a
// non-empty assembly.  A strong generated definition in the executable's
// objects replaces these; the coverage check then sees the real registry.
// When the build adds the generated assembly directly to this executable it
// also defines ACSD_HAS_GENERATED_ASSEMBLY and this fallback disappears.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
Result<rt::Done> register_builtin_types(rt::RegistryBuilder&) {
  Result<rt::Done> out;
  out.code = Code::ok;
  out.value = rt::Done{};
  return out;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
Result<rt::Done> register_processes(rt::RegistryBuilder&) {
  Result<rt::Done> out;
  out.code = Code::ok;
  out.value = rt::Done{};
  return out;
}

}  // namespace acsd::generated
#endif  // !ACSD_HAS_GENERATED_ASSEMBLY

namespace {

std::string describe(const acsd::Result<acsd::rt::Done>& result) {
  std::string message = "registration failed";
  if (!result.assessment.issues.empty()) {
    const acsd::Issue& issue = result.assessment.issues.front();
    message = issue.origin.empty() ? issue.reason : issue.origin + ": " +
                                                        issue.reason;
  }
  return message;
}

bool write_atomically(const std::filesystem::path& output,
                      std::string_view contents, std::string& error) {
  std::error_code ec;
  const std::filesystem::path parent = output.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      error = "cannot create " + parent.string() + ": " + ec.message();
      return false;
    }
  }
  std::filesystem::path temporary = output;
  temporary += ".tmp";
  // A stale temporary from an interrupted run is ours to replace.
  std::filesystem::remove(temporary, ec);
  ec.clear();
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
      error = "cannot open " + temporary.string() + ": " +
              std::strerror(errno);
      return false;
    }
    stream.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    stream.flush();
    if (!stream) {
      error = "cannot write " + temporary.string();
      stream.close();
      std::filesystem::remove(temporary, ec);
      return false;
    }
  }
  std::filesystem::rename(temporary, output, ec);
  if (ec) {
    // rename does not replace on every platform/filesystem combination.
    std::error_code remove_error;
    std::filesystem::remove(output, remove_error);
    ec.clear();
    std::filesystem::rename(temporary, output, ec);
    if (ec) {
      error = "cannot commit " + output.string() + ": " + ec.message();
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path output = "build/process-manifest.json";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--output" && index + 1 < argc) {
      output = argv[++index];
    } else if (argument == "--help" || argument == "-h") {
      std::cout << "usage: acsd-runtime-manifest [--output PATH]\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << argument << "\n";
      return 2;
    }
  }

  try {
    acsd::rt::RegistryBuilder builder;
    acsd::Result<acsd::rt::Done> step =
        generated::register_builtin_types(builder);
    if (step.code != Code::ok) {
      std::cerr << "register_builtin_types: " << describe(step) << "\n";
      return 1;
    }
    step = generated::register_processes(builder);
    if (step.code != Code::ok) {
      std::cerr << "register_processes: " << describe(step) << "\n";
      return 1;
    }
    acsd::Result<std::shared_ptr<const acsd::rt::Registry>> frozen =
        builder.freeze();
    if (frozen.code != Code::ok || !frozen.value) {
      std::string message = "freeze failed";
      if (!frozen.assessment.issues.empty())
        message = frozen.assessment.issues.front().reason;
      std::cerr << "registry freeze: " << message << "\n";
      return 1;
    }
    const std::string manifest = (*frozen.value)->manifest_json();
    std::string error;
    if (!write_atomically(output, manifest, error)) {
      std::cerr << error << "\n";
      return 1;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "manifest exporter exception: " << error.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "manifest exporter failed with an unknown exception\n";
    return 1;
  }
}
