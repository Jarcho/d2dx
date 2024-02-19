#include "pch.h"

#include "Profiler.h"
#include "Utils.h"
#include "D2DXContextFactory.h"

using namespace d2dx;
using namespace std;

#ifdef D2DX_PROFILE
static thread_local unsigned int halt_sleep_profile = 0;
static thread_local Timer* currentTimer = nullptr;

class Profiler {
public:
	void AddTime(
		_In_ int64_t time,
		_In_ ProfCategory category,
		_In_ bool fullEvent) noexcept
	{
		if (category == ProfCategory::Sleep)
		{
			if (halt_sleep_profile == 0)
			{
				auto ctxt = D2DXContextFactory::GetInstance(false);
				if (ctxt && ctxt->GetActiveThreadId() == GetCurrentThreadId())
				{
					_times[static_cast<size_t>(category)] += time;
					_times[static_cast<size_t>(ProfCategory::Count)] += time;
					if (fullEvent)
					{
						++_events[static_cast<size_t>(category)];
						++_events[static_cast<size_t>(ProfCategory::Count)];
					}
				}
				else
				{
					_atomicTime.fetch_add(time, std::memory_order_relaxed);
					if (fullEvent)
					{
						_atomicEvents.fetch_add(1, std::memory_order_relaxed);
					}
				}
			}
		}
		else
		{
			_times[static_cast<size_t>(category)] += time;
			_times[static_cast<size_t>(ProfCategory::Count)] += time;
			if (fullEvent)
			{
				++_events[static_cast<size_t>(category)];
				++_events[static_cast<size_t>(ProfCategory::Count)];
			}
		}
	}

	void WriteProfile() noexcept
	{
		double frameTime = TimeToMs(TimeStamp() - lastProfileTime);

		double hashSize = static_cast<double>(tex_miss_size);
		auto hashUnit = "B";
		if (hashSize >= 1024 * 1024) {
			hashSize /= 1024 * 1024;
			hashUnit = "MiB";
		}
		else if (hashSize >= 1024) {
			hashSize /= 1024;
			hashUnit = "kiB";
		}
		int64_t atomicTime = _atomicTime.load(memory_order_relaxed);
		uint32_t atomicEvents = _atomicEvents.load(memory_order_relaxed);

		if (frameTime > 20) {
			D2DX_LOG_PROFILE(
				"Frame profile:\n"
				"Time: %.4fms\n"
				"Profiled time: %.4fms (%u events)\n"
				"TextureDownload: %.4fms (%u events)\n"
				"TextureSource: %.4fms (%u events)\n"
				"TextureHash Miss Rate: %u/%u (%.2f%s)\n"
				"MotionPrediction: %.4fms (%u events)\n"
				"Draw: %.4fms (%u events) (%u dropped)\n"
				"DrawBatches: %.4fms\n"
				"Sleep: %.4fms (%u events)\n"
				"Sleep (other): %.4fms (%u events)\n"
				"Present: %.4fms\n",
				frameTime,
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::Count)]),
				_events[static_cast<std::size_t>(ProfCategory::Count)],
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::TextureDownload)]),
				_events[static_cast<std::size_t>(ProfCategory::TextureDownload)],
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::TextureSource)]),
				_events[static_cast<std::size_t>(ProfCategory::TextureSource)],
				tex_misses, tex_lookups, hashSize, hashUnit,
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::MotionPrediction)]),
				_events[static_cast<std::size_t>(ProfCategory::MotionPrediction)],
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::Draw)]),
				_events[static_cast<std::size_t>(ProfCategory::Draw)],
				dropped_draws,
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::DrawBatches)]),
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::Sleep)]),
				_events[static_cast<std::size_t>(ProfCategory::Sleep)],
				TimeToMs(atomicTime),
				atomicEvents,
				TimeToMs(_times[static_cast<std::size_t>(ProfCategory::Present)])
			);
		}
		memset(&_times, 0, sizeof(_times));
		memset(&_events, 0, sizeof(_events));
		_atomicTime.fetch_sub(atomicTime, memory_order_relaxed);
		_atomicEvents.fetch_sub(atomicEvents, memory_order_relaxed);
		tex_lookups = 0;
		tex_misses = 0;
		tex_miss_size = 0;
		dropped_draws = 0;
		lastProfileTime = TimeStamp();
	}

	int64_t _times[static_cast<size_t>(ProfCategory::Count) + 1] = {};
	uint32_t _events[static_cast<size_t>(ProfCategory::Count) + 1] = {};
	atomic<int64_t> _atomicTime = { 0 };
	atomic<uint32_t> _atomicEvents = { 0 };

	int64_t lastProfileTime = 0;
	size_t tex_lookups = 0;
	size_t tex_misses = 0;
	size_t tex_miss_size = 0;

	size_t dropped_draws = 0;
};

static Profiler profiler;
#endif

_Use_decl_annotations_
d2dx::Timer::Timer(
	ProfCategory category) noexcept
#ifdef D2DX_PROFILE
	: category(category)
	, start(TimeStamp())
	, parent(currentTimer)
#endif
{
#ifdef D2DX_PROFILE
	currentTimer = this;
	if (parent)
	{
		profiler.AddTime(start - parent->start, parent->category, false);
	}
#endif
}

d2dx::Timer::~Timer() noexcept
{
#ifdef D2DX_PROFILE
	profiler.AddTime(TimeStamp() - start, category, true);
	if (parent)
	{
		parent->start = TimeStamp();
	}
	currentTimer = parent;
#endif
}

d2dx::HaltSleepProfile::HaltSleepProfile() noexcept
{
#ifdef D2DX_PROFILE
	++halt_sleep_profile;
#endif
}

d2dx::HaltSleepProfile::~HaltSleepProfile() noexcept
{
#ifdef D2DX_PROFILE
	--halt_sleep_profile;
#endif
}

void d2dx::WriteProfile() noexcept
{
#ifdef D2DX_PROFILE
	profiler.WriteProfile();
#endif
}

void d2dx::AddTexHashLookup() noexcept
{
#ifdef D2DX_PROFILE
	profiler.tex_lookups += 1;
#endif
}

_Use_decl_annotations_
void d2dx::AddTexHashMiss(
	size_t size) noexcept
{
#ifdef D2DX_PROFILE
	profiler.tex_misses += 1;
	profiler.tex_miss_size += size;
#endif
}

void d2dx::AddDroppedDraw() noexcept
{
#ifdef D2DX_PROFILE
	profiler.dropped_draws += 1;
#endif
}
