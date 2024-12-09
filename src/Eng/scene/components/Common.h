#pragma once

#include <memory>

namespace Sys {
template <typename T, typename FallBackAllocator> class MultiPoolAllocator;
template <typename Alloc> struct JsObjectT;
using JsObject = JsObjectT<std::allocator<char>>;
using JsObjectP = JsObjectT<Sys::MultiPoolAllocator<char, std::allocator<char>>>;
} // namespace Sys
