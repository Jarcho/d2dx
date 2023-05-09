#pragma once

#include "Buffer.h"
#include "Types.h"

namespace d2dx
{
	namespace Metrics
	{
		Size GetSuggestedGameSize(
			_In_ Size desktopSize,
			_In_ bool wide) noexcept;
		
		Rect GetRenderRect(
			_In_ Size gameSize,
			_In_ Size desktopSize,
			_In_ bool keepAspect) noexcept;
		
		Buffer<Size> GetStandardDesktopSizes() noexcept;
	}
}
