#pragma once
// Generated registry assembly entry points.
//
// These are produced by the build from the explicit module list; they are the
// only place where modules are assembled into a registry.  They are not a
// second handwritten registry: each module still owns its ProcessDescriptor and
// typed Operation, and the coverage truth remains the process whitelist plus
// the frozen registry manifest.
//
// The build may generate an empty assembly while modules have no registration
// source yet; the manifest is then valid but empty and the coverage check
// reports the missing processes as an intermediate state.
#include <acsd/runtime/process.hpp>

namespace acsd::generated {

// Registers the shared model/Input TypeCodec bindings.  Every assembly calls it
// before the process descriptors, exactly once per builder.
Result<rt::Done> register_builtin_types(rt::RegistryBuilder& builder);

// Registers every compiled module descriptor and its typed operations.
Result<rt::Done> register_processes(rt::RegistryBuilder& builder);

}  // namespace acsd::generated
