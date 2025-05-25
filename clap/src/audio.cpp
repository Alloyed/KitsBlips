#include "audio.h"

namespace Audio {
	void Render(uint32_t start, uint32_t end, float *outputL, float *outputR) {
		for (uint32_t index = start; index < end; index++) {
			float sum = 0.0f;

			outputL[index] = sum;
			outputR[index] = sum;
		}
	}
};
