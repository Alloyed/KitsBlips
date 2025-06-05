#pragma once

#include <cstring>
#include <string_view>
#include "kitdsp/math/util.h"
    
/*
 * Utilities for working with c strings, std::string and std::string_view
 * Don't invent your own string types!
 */
namespace kitdsp {
/* safe alternative to strcpy when the buffer size is known at compile time*/
template <size_t BUFFER_SIZE>
void StringCopy(char (&buffer)[BUFFER_SIZE], std::string_view src) {
    static_assert(BUFFER_SIZE > 0);
    // zero-out memory. This implicitly null-terminates, and avoids accidental garbage lying around
    std::memset(buffer, '\0', BUFFER_SIZE);
    // copy at most BUFFER_SIZE-1 bytes. the last byte is reserved for the null terminator
    std::memcpy(buffer, src.data(), kitdsp::min(src.length(), BUFFER_SIZE - 1));
}
}  // namespace kitdsp