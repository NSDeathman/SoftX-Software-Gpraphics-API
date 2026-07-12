/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../include/SoftX.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class SOFTX_API ThreadPool
{
  public:
	ThreadPool(size_t numThreads) : stop(false), activeTasks(0)
	{
		for (size_t i = 0; i < numThreads; ++i)
		{
			workers.emplace_back([this, i] {
				PROFILE_THREAD(("SoftX Worker " + std::to_string(i)).c_str());
				while (true)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(queueMutex);
						condition.wait(lock, [this] { return stop || !tasks.empty(); });
						if (stop && tasks.empty())
							return;

						task = std::move(tasks.front());
						tasks.pop();
						++activeTasks;
					}

					task();

					{
						std::unique_lock<std::mutex> lock(queueMutex);
						--activeTasks;
						condition.notify_all();
					}
				}
			});
		}
	}

	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stop = true;
		}
		condition.notify_all();
		for (auto& worker : workers)
			worker.join();
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

	template <class F> void enqueue(F&& task)
	{
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			tasks.emplace(std::forward<F>(task));
		}
		condition.notify_one();
	}

	void wait()
	{
		PROFILE_SCOPE("Waiting for workers");
		PROFILE_TAG("Blocked", "true");
		std::unique_lock<std::mutex> lock(queueMutex);
		condition.wait(lock, [this] { return tasks.empty() && activeTasks == 0; });
	}

	size_t threadCount() const
	{
		return workers.size();
	}

  private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;
	std::mutex queueMutex;
	std::condition_variable condition;
	std::atomic<int> activeTasks;
	bool stop;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
