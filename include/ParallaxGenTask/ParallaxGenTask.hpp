#pragma once

#include <string>
#include <unordered_map>

class ParallaxGenTask
{
public:
    enum class PGResult
    {
        SUCCESS,
        SUCCESS_WITH_WARNINGS,
        FAILURE
    };

private:
    std::string task_name;
    size_t total_jobs;
    size_t last_perc = 0;

    std::unordered_map<PGResult, size_t> num_jobs_completed;

    std::unordered_map<PGResult, std::string> pg_result_str = {
        {PGResult::SUCCESS, "COMPLETED"},
        {PGResult::SUCCESS_WITH_WARNINGS, "COMPLETED WITH WARNINGS"},
        {PGResult::FAILURE, "FAILED"}
    };

public:
    ParallaxGenTask(const std::string& task_name, const size_t total_jobs);

    void completeJob(const PGResult& result);
    void initJobStatus();
    void printJobStatus(bool force = false);
    void printJobSummary();
    size_t getCompletedJobs();

    static void updatePGResult(PGResult& result, const PGResult& current_result, const PGResult& threshold = PGResult::FAILURE);
};
