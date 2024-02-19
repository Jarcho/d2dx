#pragma once

namespace d2dx {
	enum class ProfCategory {
		TextureSource,
		MotionPrediction,
		Draw,
		DrawBatches,
		TextureDownload,
		Sleep,
		Present,
		Count
	};

	class Timer final {
	public:
		Timer(
			_In_ ProfCategory category) noexcept;
		~Timer() noexcept;

	private:
#ifdef D2DX_PROFILE
		Timer* parent = nullptr;
		ProfCategory category;
		int64_t start;
#endif
	};

	class HaltSleepProfile final {
	public:
		HaltSleepProfile() noexcept;
		~HaltSleepProfile() noexcept;
	};

	void WriteProfile() noexcept;

	void AddTexHashLookup() noexcept;
	void AddTexHashMiss(
		_In_ size_t size) noexcept;

	void AddDroppedDraw() noexcept;
}
