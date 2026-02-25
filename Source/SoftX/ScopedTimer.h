#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <iostream>

// —труктура дл€ хранени€ одного замера
struct TimerRecord
{
	std::string name;
	double elapsedMs; // врем€ в миллисекундах
};

//  ласс таймера с автоматическим замером времени жизни
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
