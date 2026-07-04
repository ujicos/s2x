#pragma once

#include <string_view>

namespace game::lookup::dvars
{
	std::string_view resolve_engine_name(std::string_view name);
	std::string_view resolve_display_name(std::string_view name);
}
