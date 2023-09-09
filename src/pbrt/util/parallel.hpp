#ifndef PBRTV4_SRC_PBRT_UTIL_PARALLEL_HPP_
#define PBRTV4_SRC_PBRT_UTIL_PARALLEL_HPP_

#include <pbrt/pbrt.h>

#include <pbrt/util/float.h>
#include <pbrt/util/vecmath.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <initializer_list>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace pbrt {

//threadlocal definition
template <typename T>
class ThreadLocal {
 public:
	//threadlocal public methods
	ThreadLocal() : hashTable(4 * RunningThreads()), create([]() { return T(); }) {}
	ThreadLocal(std::function<T(void)> &&c) : hashTable(4 * RunningThreads()), create(c) {}

	T &Get();

	template <typename F>
	void ForAll(F &&func);

 private:
	//threadlocal private members

};

class ThreadPool;

class ParallelJob {
 public:
	//paralleljob public methods
	virtual bool HaveWork() const = 0;
	virtual void RunStep(std::unique_lock<std::mutex> *lock) = 0;

	bool Finished() const { return !HaveWork() && activeWorkers == 0; }

	//paralleljob public members
	static ThreadPool *threadPool;

 private:
	//paralleljob private members
	friend class ThreadPool;
	int activeWorkers = 0;
	ParallelJob *prev = nullptr, *next = nullptr;
	bool removed = false;
};

class ThreadPool {
 public:
	//threadpool public methods
	explicit ThreadPool(int nThreads);

	std::unique_lock<std::mutex> AddToJobList(ParallelJob *job);

	void WorkOrWait(std::unique_lock<std::mutex> *lock, bool isEnqueuingThread);
	bool WorkOrReturn();

	void ForEachThread(std::function<void(void)> func);

	std::unique_lock<std::mutex> AddToJobList(ParallelJob *job);
	void RemoveFromJobList(ParallelJob *job);

	void Disable();
	void Reenable();

 private:
	//threadpool private methods
	void Worker();

	//threadpool private members
	std::vector<std::threads> threads;
	mutable std::mutex mutex;
	bool shutdownThreads = false;
	bool disabled = false;

	ParallelJob *jobList = nullptr;
	ParallelJob *prev = nullptr, *next = nullptr;

	std::condition_variable jobListCondition;
};

void RunStep(std::unique_lock<std::mutex> *lock);

void ParallelFor(int64_t start, int64_t end, std::function<void(int64_t, int64_t> func);

//parallel inline functions
inline void ParallelFor(int64_t start, int64_t end, std::function<void(int64_t)> func)
{
	ParallelFor(start, end, [&func](int64_t start, int64_t end)
	{
		for (int64_t i = start; i < end; i++)
			func(i);
	});
}

inline void ParallelFor2D(const Bounds2i &extent, std::function<void(Point2i)> func)
{
	ParallelFor2D(extent, [&func](Bounds2i b)
	{
	  for (Point2i p : b)
		  func(p);
	});
}

bool DoParallelWork();

//AsyncJob Definition
template <typename T>
class AsyncJob : public ParallelJob {
 public:
	//asyncjob public methods
	AsyncJob(std::function<T(void)> w) : func(std::move(w)) {}

	bool HaveWork() const { return !started; }

	void RunStep(std::unique_lock<std::mutex> *lock)
	{
		threadPool->RemoveFromJobList(this);
		started = true;
		lock->unlock;
		//execute asynchronous work and notify waiting threads of its completion
		T r = func();
		std::unique_lock<std::mutex> ul(mutex);
		result = r;
		cv.notify_all();
	}

	bool IsReady() const {
		std::lock_guard<std::mutex> lock(mutex);
		return result.has_value();
	}

	T GetResult()
	{
		Wait();
		std::lock_guard<std::mutex> lock(mutex);
		return *result;
	}

	pstd::optional<T> TryGetResult(std::mutex *extMutex)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (result)
				return result;
		}

		extMutex->unlock();
		DoParallelWork();
		extMutex->lock();
		return {};
	}

	void Wait()
	{
		//???
		while(!IsReady() && DoParallelWork())
			;
		std::unique_lock<std::mutex> lock(mutex);
		if(!result.has_value())
			cv.wait(lock, [this]() { return result.has_value(); });
	}

	void DoWork()
	{
		T r = func();
		std::unique_lock<std::mutex> l(mutex);
		result = r;
		cv.notify_all();
	}

 private:
	//asyncjob private members
	std::function<T(void)> func;
	bool started = false;

	pstd::optional<T> result;
	mutable syd::mutex mutex;
	std::condition_variable cv;
};

template <typename F, typename... Args>
auto RunAsync(F func, Args &&...args)
{
	//create asyncjob for func and args
	auto fvoid = std::bind(func, std::forward<Args>(args)...);
	using R = typename std::invoke_result_t<F, Args..>;
	AsyncJob<R> *job = new AsyncJob<R>(std::move(fvoid));

	//enqueue job or run it immediately
	std::unique_lock<std::mutex> lock;
	if(RunningThreads() == 1)
		job->DoWork();
	else
		lock = ParallelJob::threadPool->AddToJobList(job);

	return job;
}

//AtomicFloat Definition
class AtomicFloat {
 public:
	//AtomicFloat public methods
	explicit AtomicFloat(float v = 0)
	{
		bits = FloatToBits(v);
	}

	operator float() const
	{
		return BitsToFloat(bits);
	}
	Float operator=(float v)
	{
		bits = FloatToBits(v);
		return v;
	}

	void Add(float v)
	{
		FloatBits oldBits = bits, newBits;
		do {
			newBits = FloatToBits(BitsToFloat(oldBits) + v);
		} while (!bits.compare_exchange_weak(oldBits, newBits));
	}

 private:
	//AtomicFloat private members
	std::atomic<FloatBits> bits;
};

//AtomicDouble Definition
class AtomicDouble {
 public:
	//AtomicDouble public methods
	explicit AtomicDouble(float v = 0)
	{
		bits = FloatToBits(v);
	}

	operator float() const
	{
		return BitsToFloat(bits);
	}
	double operator=(double v)
	{
		bits = FloatToBits(v);
		return v;
	}

	void Add(double v)
	{
		uint64_t oldBits = bits, newBits;
		do {
			newBits = FloatToBits(BitsToFloat(oldBits) + v);
		} while (!bits.compare_exchange_weak(oldBits, newBits));
	}

 private:
	//AtomicDouble private members
	std::atomic<uint64_t> bits;
};

int AvailableCores();
int RunningThreads();

} //namespace

#endif //PBRTV4_SRC_PBRT_UTIL_PARALLEL_HPP_
