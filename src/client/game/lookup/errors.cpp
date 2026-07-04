#include <std_include.hpp>

#include "errors.hpp"
#include "lookup.hpp"

namespace game::lookup::errors
{
	namespace
	{
		const detail::string_map& error_formats()
		{
			static const detail::string_map values
			{
				{ "1823", "Assert fail" },
				{ "1824 %s", "Assert fail: %s" },
				{ "1825 %s", "Assert fail: %s" },
				{ "2701", "Function requires at least 1 parameter" },
				{ "2777", "Function requires exactly 1 parameter" },
				{ "2804", "Function requires exactly 2 parameters" },
				{ "3156", "setnodepriority requires exactly 2 parameters" },
				{ "3157", "setnodepriority called on an unsupported node type. Valid node types:" },
				{ "3159", "isnodeoccupied requires exactly 1 parameter" },
				{ "3161", "setturretnode requires exactly 2 parameters" },
				{ "3162", "First parameter must be a turret node" },
				{ "3163", "Second parameter must be a turret entity" },
				{ "3164 %d", "Turret node is already assigned to entity %d" },
				{ "3165 %d", "Too many turret nodes; maximum is %d" },
				{ "3166", "unsetturretnode requires exactly 1 parameter" },
				{ "3167", "Parameter must be a turret node" },
				{ "3611 %u", "Exceeded maximum notifyoncommand registrations: %u" },
				{ "3705 %s", "Type %s is not an int" },
				{ "3706 %u", "Parameter %u does not exist" },
				{ "3709 %s", "Type %s is not a float" },
				{ "3710 %u", "Parameter %u does not exist" },
				{ "3711 %u", "Parameter %u does not exist" },
				{ "3712 %u", "Parameter %u does not exist" },
				{ "3713 %u", "Parameter %u does not exist" },
				{ "3714 %s", "Type %s is not a localized string" },
				{ "3716 %s", "Type %s is not a vector" },
				{ "3717 %u", "Parameter %u does not exist" },
				{ "3723 %s", "Type %s is not an object" },
				{ "3726 %u", "Parameter %u does not exist" },
				{ "3727 %u", "Parameter %u does not exist" },
				{ "3728 %u", "Parameter %u does not exist" },
				{ "4089 %s", "Illegal localized string reference: %s must contain only alpha-numeric characters and underscores" },
				{ "4260 %s", "Type %s is not an array" },
				{ "4680 %s", "Type %s is not an object" },
			};

			return values;
		}
	}

	std::string_view translate_format(const std::string_view fmt)
	{
		return detail::find_or_self(error_formats(), fmt);
	}

	std::string_view translate_format(const char* fmt)
	{
		if (!fmt)
		{
			return "";
		}

		return translate_format(std::string_view{ fmt });
	}
}
