#pragma once

#include <cstdint>

namespace Audio {
	void Render(uint32_t start, uint32_t end, float *outputL, float *outputR);
}
