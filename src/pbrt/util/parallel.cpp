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

