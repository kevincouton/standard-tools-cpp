#pragma once

#include "standard_tools/api/state.hpp"

#include <crow.h>

namespace standard_tools::api {

crow::App<>& RegisterMCPRoutes(crow::App<>& app, AppState& state);

}  // namespace standard_tools::api
