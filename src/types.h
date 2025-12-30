#pragma once

#include <simdjson.h>

namespace trade_connector {

using Json = simdjson::simdjson_result<simdjson::fallback::ondemand::document>;
using Callback = void(*)(const Json&);

} // namespace trade_connector