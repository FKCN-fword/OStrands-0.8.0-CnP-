#pragma once

namespace OStrands::SexLabBridge
{
	void Connect();
	void Update(float a_delta);
	void Reset();
	bool Owns(std::int32_t a_thread);
}
