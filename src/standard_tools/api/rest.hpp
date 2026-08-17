#pragma once

#include "standard_tools/api/auth.hpp"
#include "standard_tools/api/state.hpp"

#include <crow.h>

namespace standard_tools::api {

App& RegisterRoutes(App& app, AppState& state);

}  // namespace standard_tools::api
