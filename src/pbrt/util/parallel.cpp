#include "parallel.hpp"

ThreadPool::ThreadPool(int nThreads)
{
	for(int i = 0; i < nThreads-1; i++)
		threads.push_back(std::thread(&ThreadPool::Worker, this));
}

void ThreadPool::Worker()
{
	std::unique_lock<std::mutex> lock(mutex);
	while(!shutdownThreads)
		WorkOrWait(&lock, false);
}

std::unique_lock<std::mutex> ThreadPool::AddToJobList(ParallelJob* job)
{
	std::unique_lock<std::mutex> lock(mutex);
	//add job to head of joblist
	if(jobList)
		jobList->prev = job;
	job->next = jobList;
	jobList = job;

	jobListCondition.notify_all();
	return lock;
}

void ThreadPool::WorkOrWait(std::unique_lock<std::mutex> *lock, bool isEnqueuingThread)
{
	//return if this is a worker thread and the thread pool is disabled
	if(!isEnqueuingThread && disabled)
	{
		jobListCondition.wait(*lock);
		return;
	}

	ParallelJob *job = jobList;
	while(job && !job->HaveWork())
		job = job->next;
	if(job)
	{
		//execute work for job
		job->activeWorkers++;
		job->RunStep(lock);
		//handle post-job-execution details
		lock->lock();
		job->activeWorkers--;
		if(job->Finished())
			jobListCondition.notify_all();

	} else {
		//wait for new work to arrive or the job to finish
		jobListCondition.wait(*lock);
	}
}

bool ThreadPool::WorkOrReturn()
{
	std::unique_lock<std::mutex> lock(mutex);

	ParallelJob *job = jobList;
	while(job && !job->HaveWork())
		job = job->next;
	if(!job)
		return false;

	//execute work for job
	job->activeWorkers++;
	job->RunStep(&lock);
	//handle post-job-execution details
	lock.lock();
	job->activeWorkers--;

	if(job->Finished())
		jobListCondition.notify_all();


	return true;
}

void ThreadPool::ForEachThread(std::function<void(void)> func)
{
	Barrier *barrier = new Barrier(threads.size() + 1);

	ParallelFor(0, threads.size() + 1, [barrier, &func](int64_t) {
		func();
		if(barrier->Block())
			delete barrier;
	});
}

std::unique_lock<std::mutex> ThreadPool::AddToJobList(ParallelJob *job) {
	std::unique_lock<std::mutex> lock(mutex);
	// Add _job_ to head of _jobList_
	if (jobList)
		jobList->prev = job;
	job->next = jobList;
	jobList = job;

	jobListCondition.notify_all();
	return lock;
}

void ThreadPool::RemoveFromJobList(ParallelJob *job)
{
	if(job->prev)
		job->prev->next = job->next;
	else
		jobList = job->next;
	if(job->next)
		job->next->prev = job->prev;
}

void ThreadPool::Disable() {
	disabled = true;
}

void ThreadPool::Reenable()
{
	disabled = false;
}

//parallelforloop1d
class ParallelForLoop1D : public ParallelJob
{
 public:
	//parallelforloop1d public methods
	ParallelForJob1D(int64_t startIndex, int64_t endIndex, int chunkSize, std::function<void(int64_t, int64_t)> func) :
		func(std::move(func)), nextIndex(startIndex), endIndex(endIndex), chunkSize(chunkSize) {}

	bool HaveWork() const { return nextIndex < endIndex; }

	void RunStep(std::unique_lock<std::mutex> *lock);

 private:
	//parallelforloop1d private members
	std::function<void(int64_t, int64_t)> func;
	int64_t nextIndex, endIndex;
	int chunkSize;
};

void RunStep(std::unique_lock<std::mutex> *lock)
{
	//determine the range of loop iterations to run in this step
	int64_t indexStart = nextIndex;
	int64_t indexEnd = std::min(indexStart + chunkSize, endIndex);
	nextIndex = indexEnd;

	//remove job from the list if all work has been started
	if(!HaveWork())
		threadPool->RemoveFromJobList(this);

	//release lock and execute loop iterations in [indexStart, indexEnd)
	lock->unlock();
	func(indexStart, indexEnd);
}

void ParallelFor(int64_t start, int64_t end, std::function<void(int64_t, int64_t> func)
{
	if(start == end)
		return;

	//compute chunk size for parallel loop
	int64_t chunkSize = std::max<int64_t>(1, (end - start) / (8 * RunningThreads()));
	//create and enqueue ParallelForLoop1D for this loop
	ParallelForLoop1D loop(start, end, chunkSize, std::move(func));
	std::unique_lock<std::mutex> lock = ParallelJob::threadPool->AddToJobList(&loop);
	//Help out with parallel loop iterations in the current thread
	while(!loop.Finished())
		ParallelJob::threadPool->WorkOrWait(&lock, true);
}

bool DoParallelWork() {
	// lock should be held when this is called...
	return ParallelJob::threadPool->WorkOrReturn();
}

///////////////////////////
int AvailableCores()
{
	return std::max<int>(1, std::thread::hardware_concurrency());
}

int RunningThreads()
{
	return ParallelJob::threadPool ? (1 + ParallelJob::threadPool->size()) : 1;
}
