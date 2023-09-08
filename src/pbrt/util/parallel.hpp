#ifndef PBRTV4_SRC_PBRT_UTIL_PARALLEL_HPP_
#define PBRTV4_SRC_PBRT_UTIL_PARALLEL_HPP_

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

#endif //PBRTV4_SRC_PBRT_UTIL_PARALLEL_HPP_
