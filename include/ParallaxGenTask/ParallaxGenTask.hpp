#pragma once

#include <string>

class ParallaxGenTask
{
private:
    std::string task_name;
    size_t total_jobs;
    size_t job_count_success = 0;
    size_t job_count_failure = 0;
    size_t last_perc = 0;

public:
    ParallaxGenTask(const std::string& task_name, const size_t total_jobs);

    void completeJobSuccess();
    void completeJobFailure();
    void initJobStatus();
    void printJobStatus(bool force = false);
    void printJobSummary();
};