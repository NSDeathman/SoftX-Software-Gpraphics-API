#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
  public:
	ThreadPool(size_t numThreads) : stop(false), activeTasks(0)
	{
		for (size_t i = 0; i < numThreads; ++i)
		{
			workers.emplace_back([this] {
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
					}
					activeTasks++;
					task();
					activeTasks--;
					completedCondition.notify_one();
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
		for (std::thread& worker : workers)
		{
			worker.join();
		}
	}

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
		std::unique_lock<std::mutex> lock(completedMutex);
		completedCondition.wait(lock, [this] { return activeTasks == 0 && tasks.empty(); });
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
	bool stop;
	std::atomic<int> activeTasks;
	std::condition_variable completedCondition;
	std::mutex completedMutex;
};
