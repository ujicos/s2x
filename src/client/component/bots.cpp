#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "command.hpp"
#include "scheduler.hpp"

#include "game/game.hpp"

#include "console/console.hpp"

#include <utils/hook.hpp>

namespace bots
{
	namespace
	{

	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			utils::hook::set(game::mp::BG_BotFastFileEnabled, 0xC301B0);
			utils::hook::set(game::mp::BG_BotsUsingTeamDifficulty, 0xC301B0);
			utils::hook::set(game::mp::BG_BotSystemEnabled, 0xC301B0);
			utils::hook::set(game::mp::BG_AgentSystemEnabled, 0xC301B0);
			
			// Not sure
			utils::hook::set(0x388210_g, 0xC301B0);

			command::add("SpawnBot", [](const command::params& params)
			{
				if (!game::SV_Loaded() || *game::mp::virtualLobby_Loaded)
				{
					return;
				}

				//TODO: Check how many spots left to fill with bots

				size_t count = 1;
				if (params.size() > 1)
				{
					count = atoi(params[1]);
				}

				scheduler::once([count]
				{
					for (size_t i = 0; i < count; ++i)
					{
						auto* ent = game::mp::SV_AddBot("", 1);
						game::mp::SV_SpawnTestClient(ent);
					}
				}, scheduler::server);				
			});
		}
	};
}

REGISTER_COMPONENT(bots::component)
