#pragma once

#include "standard_tools/api/state.hpp"

#include <crow.h>

namespace standard_tools::api {

crow::App<>& RegisterRoutes(crow::App<>& app, AppState& state);

}  // namespace standard_tools::api
