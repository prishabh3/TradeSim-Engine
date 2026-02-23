#pragma once

#include <string>
#include <cstdint>

namespace trading {

// Re-export order types here for engine-internal use
// (full structs live in common/Types.hpp)

enum class OrderType   { MARKET, LIMIT };
enum class OrderSide   { BUY, SELL };
enum class OrderStatus { PENDING, PARTIAL, FILLED, CANCELLED, REJECTED };

} // namespace trading
