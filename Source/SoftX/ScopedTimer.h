#pragma once
#include <vector>
#include <string>
#include <chrono>
#include <windows.h> // для OutputDebugStringA

struct TimerRecord
{
	std::string name;
	double elapsedMs;
};

class ScopedTimer
{
  public:
	ScopedTimer(const std::string& name, std::vector<TimerRecord>& records)
		: m_name(name), m_records(records), m_start(std::chrono::high_resolution_clock::now())
	{
	}

	~ScopedTimer()
	{
		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration<double, std::milli>(end - m_start).count();
		m_records.push_back({m_name, elapsed});
	}

  private:
	std::string m_name;
	std::vector<TimerRecord>& m_records;
	std::chrono::high_resolution_clock::time_point m_start;
};

// Глобальный вектор (объявление, определение в одном cpp)
extern std::vector<TimerRecord> g_timers;

// Макросы
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#ifdef _MSC_VER
#define PROFILE_SCOPE(name) ScopedTimer CONCAT(profile_timer_, __COUNTER__)(name, g_timers)
#else
#define PROFILE_SCOPE(name) ScopedTimer CONCAT(profile_timer_, __LINE__)(name, g_timers)
#endif

#define LOG_PROFILE()                                                                                                  \
	for (const auto& rec : g_timers)                                                                                   \
	{                                                                                                                  \
		char buf[256];                                                                                                 \
		sprintf_s(buf, sizeof(buf), "%s: %.3f ms\n", rec.name.c_str(), rec.elapsedMs);                                 \
		OutputDebugStringA(buf);                                                                                       \
	}

#define PREPARE_TIMERS g_timers.clear