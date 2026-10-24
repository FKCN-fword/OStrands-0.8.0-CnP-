#pragma once

#include "Anatomy.h"

namespace OStrands::AutoPairs
{
	class Detector
	{
	public:
		void                     Begin(std::vector<RE::ActorHandle> a_actors);
		bool                     Sample(float a_delta);
		const std::vector<Pair>& Pairs() const { return _pairs; }
		std::string              Describe() const;

	private:
		struct Candidate
		{
			Pair          pair;
			std::uint32_t tool{ 0 };
			std::uint32_t target{ 0 };
			bool          shaft{ false };
			float         sum{ 0.0f };
			bool          active{ false };
		};

		Candidate& Find(const Pair& a_pair, std::uint32_t a_tool, std::uint32_t a_target, bool a_shaft);

		std::vector<RE::ActorHandle> _actors;
		std::vector<Candidate>       _candidates;
		std::vector<Pair>            _pairs;
		float                        _clock{ 0.0f };
		float                        _pending{ 0.0f };
		std::uint32_t                _samples{ 0 };
	};
}
