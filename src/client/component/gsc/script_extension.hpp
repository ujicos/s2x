#pragma once
#include <xsk/gsc/engine/s2.hpp>

namespace gsc
{
#pragma pack(push, 1)
	struct dev_map_instruction
	{
		std::uint32_t codepos;
		std::uint16_t line;
		std::uint16_t col;
	};

	struct dev_map
	{
		std::uint32_t num_instructions;
		dev_map_instruction instructions[1];
	};
#pragma pack(pop)

	struct devmap_entry
	{
		const std::uint8_t* bytecode;
		std::size_t size;
		std::string script_name;
		std::vector<dev_map_instruction> devmap;
	};

	void add_devmap_entry(std::uint8_t*, std::size_t, const std::string&, xsk::gsc::buffer);
	void clear_devmap();

	void scr_error(const char* error);
	void override_function(const std::string& name, game::BuiltinFunction func);
	void add_function(const std::string& name, game::BuiltinFunction function);
}
