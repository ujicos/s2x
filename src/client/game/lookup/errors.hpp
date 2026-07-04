#pragma once

#include <string_view>

namespace game::lookup::errors
{
	std::string_view translate_format(const char* fmt);
	std::string_view translate_format(std::string_view fmt);
}
