#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "command.hpp"

#include "game/game.hpp"
#include "game/dvars.hpp"

#include "console/console.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>

namespace party
{
	namespace
	{

	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			command::add("map_restart", []()
			{
				*game::sv_map_restart = 1;
				*game::sv_loadScripts = 1;
				*game::mp::sv_migrate = 0;

				game::mp::SV_MapRestart(*game::mp::sv_migrate, *game::sv_loadScripts);
			});

			command::add("fast_restart", []()
			{
				game::SV_FastRestart_f();
			});

			command::add("map", [](const command::params& argument)
			{
				if (argument.size() < 2)
				{
					console::info("usage: map <mapname> [gametype]: loads a map with an optional gametype\n");
					return;
				}

				const auto com_sv_running = game::Dvar_FindMalleableVar("1080");
				if (com_sv_running && com_sv_running->current.enabled &&
					(game::SV_Loaded() && !*game::mp::virtualLobby_Loaded))
				{
					// TODO: implement mid match map changing.
					return;
				}

				const std::string map_name = argument[1];

				const auto* g_gametype = game::Dvar_FindMalleableVar("1924"); // g_gametype
				const bool has_gametype = argument.size() >= 3;

				const std::string gametype = has_gametype
					? argument[2]
					: (g_gametype ? g_gametype->current.string : "dm");

				const auto map_index = game::UI_GetListIndexFromMapName(map_name.data());
				if (map_index < 0)
				{
					console::error("Map '%s' not found in UI list.\n", map_name.data());
					return;
				}

				console::info("Starting map '%s' index %d gametype '%s'\n",
					map_name.data(),
					map_index,
					gametype.data());

				game::UI_SetMap(map_name.data(), gametype.data());

				if (has_gametype)
				{
					game::Dvar_SetStringByName("1924", gametype.data()); // g_gametype
				}

				game::Dvar_SetStringByName("1673", map_name.data()); // mapname
				game::Dvar_SetIntByName("864", map_index);           // ui_mapname
				*game::mp::sv_migrate = 0;

				const auto* args = "StartServer";
				game::UI_RunMenuScript(0, &args);
			});
		}
	};
}

REGISTER_COMPONENT(party::component)
