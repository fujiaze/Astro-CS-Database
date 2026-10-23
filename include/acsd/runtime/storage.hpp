#pragma once
// Asynchronous array read / append / seal / abort on the unique I/O lanes.
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/task.hpp>

namespace acsd::rt {

class ArrayWriter;
using WriterRef = std::shared_ptr<ArrayWriter>;

Task<WriterRef> begin_async(InvocationContext, ArenaRef, BuildSpec);

template <class T>
Task<Block<T>> read_async(InvocationContext, ArenaRef, ArrayRef, BlockRequest);

template <class T>
Task<U64> append_async(InvocationContext, WriterRef, Block<T>);

Task<ArrayRef> seal_async(InvocationContext, WriterRef);
Task<Done> abort_async(InvocationContext, WriterRef);

}  // namespace acsd::rt
