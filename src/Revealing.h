#pragma once

namespace OStrands::Revealing
{
	bool        Exposed(RE::Actor* a_actor);
	void        Mark(bool a_revealing);
	void        Toggle();
	std::string Status();
}
