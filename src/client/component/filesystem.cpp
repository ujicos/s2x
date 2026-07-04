#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "filesystem.hpp"
#include "console/console.hpp"

#include "game/game.hpp"

#include <utils/io.hpp>

namespace filesystem
{
	namespace
	{
		bool initialized = false;

		std::deque<std::filesystem::path>& get_search_paths_internal()
		{
			static std::deque<std::filesystem::path> search_paths{};
			return search_paths;
		}

		bool can_insert_path(const std::filesystem::path& path)
		{
			const auto normalized = path.lexically_normal();

			for (const auto& path_ : get_search_paths_internal())
			{
				if (path_.lexically_normal() == normalized)
				{
					return false;
				}
			}

			return true;
		}

		void startup()
		{
			if (initialized)
			{
				return;
			}

			initialized = true;

			const auto base = std::filesystem::current_path();

			register_path(base / "main");
			register_path(base / "raw");
			register_path(base / "raw_shared");
			register_path(base / "devraw_shared");
			register_path(base / "devraw");

			// Client custom folders
			register_path(base / "s2x");
			register_path(game::get_appdata_path() / "data");
		}

		void check_for_startup()
		{
			if (!initialized)
			{
				startup();
			}
		}
	}

	std::string read_file(const std::string& path)
	{
		std::string data{};
		read_file(path, &data);
		return data;
	}

	bool read_file(const std::string& path, std::string* data, std::string* real_path)
	{
		check_for_startup();

		for (const auto& search_path : get_search_paths_internal())
		{
			const auto path_ = search_path / path;

			if (utils::io::read_file(path_.generic_string(), data))
			{
				if (real_path)
				{
					*real_path = path_.generic_string();
				}

				return true;
			}
		}

		return false;
	}

	bool find_file(const std::string& path, std::string* real_path)
	{
		check_for_startup();

		for (const auto& search_path : get_search_paths_internal())
		{
			const auto path_ = search_path / path;

			if (utils::io::file_exists(path_.generic_string()))
			{
				if (real_path)
				{
					*real_path = path_.generic_string();
				}

				return true;
			}
		}

		return false;
	}

	bool exists(const std::string& path)
	{
		std::string real_path{};
		return find_file(path, &real_path);
	}

	void register_path(const std::filesystem::path& path)
	{
		const auto normalized = path.lexically_normal();

		if (can_insert_path(normalized))
		{
			console::debug("[FS] Registering path '%s'\n", normalized.generic_string().data());
			get_search_paths_internal().push_front(normalized);
		}
	}

	void unregister_path(const std::filesystem::path& path)
	{
		check_for_startup();

		const auto normalized = path.lexically_normal();
		auto& search_paths = get_search_paths_internal();

		for (auto i = search_paths.begin(); i != search_paths.end();)
		{
			if (i->lexically_normal() == normalized)
			{
				console::debug("[FS] Unregistering path '%s'\n", i->generic_string().data());
				i = search_paths.erase(i);
			}
			else
			{
				++i;
			}
		}
	}

	std::vector<std::string> get_search_paths()
	{
		check_for_startup();

		std::vector<std::string> paths{};

		for (const auto& path : get_search_paths_internal())
		{
			paths.push_back(path.generic_string());
		}

		return paths;
	}

	std::vector<std::string> get_search_paths_rev()
	{
		check_for_startup();

		std::vector<std::string> paths{};
		const auto& search_paths = get_search_paths_internal();

		for (auto i = search_paths.rbegin(); i != search_paths.rend(); ++i)
		{
			paths.push_back(i->generic_string());
		}

		return paths;
	}

	class component final : public generic_component
	{
	public:
		void post_unpack() override
		{
			startup();
		}
	};
}

REGISTER_COMPONENT(filesystem::component)
