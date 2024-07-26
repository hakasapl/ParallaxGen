#include "ParallaxGenTask/ParallaxGenTask.hpp"

#include <spdlog/spdlog.h>

ParallaxGenTask::ParallaxGenTask(const std::string& task_name, const size_t total_jobs)
{
    this->task_name = task_name;
    this->total_jobs = total_jobs;

    initJobStatus();
}

void ParallaxGenTask::completeJobSuccess()
{
    job_count_success++;
    printJobStatus();
}

void ParallaxGenTask::completeJobFailure()
{
    job_count_failure++;
    printJobStatus();
}

void ParallaxGenTask::initJobStatus()
{
    job_count_success = 0;
    job_count_failure = 0;
    last_perc = 0;

    spdlog::info("{} Starting...", task_name);
}

void ParallaxGenTask::printJobStatus(bool force)
{
    size_t combined_jobs = job_count_success + job_count_failure;
    size_t perc = combined_jobs * 100 / total_jobs;
    if (force || perc != last_perc) {
        last_perc = perc;
        spdlog::info("{} Progress: {}/{} [{}%]", task_name, combined_jobs, total_jobs, perc);
    }

    if (perc == 100) {
        printJobSummary();
    }
}

void ParallaxGenTask::printJobSummary()
{
    spdlog::info("{} Summary: {} succeeded, {} failed. See log to see error messages, if any.", task_name, job_count_success, job_count_failure);
}
