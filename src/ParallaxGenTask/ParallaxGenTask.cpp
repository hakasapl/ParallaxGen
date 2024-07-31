#include "ParallaxGenTask/ParallaxGenTask.hpp"

#include <spdlog/spdlog.h>

using namespace std;

ParallaxGenTask::ParallaxGenTask(const std::string &task_name,
                                 const size_t total_jobs) {
  this->task_name = task_name;
  this->total_jobs = total_jobs;

  initJobStatus();
}

void ParallaxGenTask::completeJob(const PGResult &result) {
  // Check if num_jobs_completed has result for key
  if (num_jobs_completed.find(result) == num_jobs_completed.end()) {
    num_jobs_completed[result] = 0;
  }

  num_jobs_completed[result]++;
  printJobStatus();
}

void ParallaxGenTask::initJobStatus() {
  last_perc = 0;

  spdlog::info("{} Starting...", task_name);
}

void ParallaxGenTask::printJobStatus(bool force) {
  size_t combined_jobs = getCompletedJobs();
  size_t perc = combined_jobs * 100 / total_jobs;
  if (force || perc != last_perc) {
    last_perc = perc;
    spdlog::info("{} Progress: {}/{} [{}%]", task_name, combined_jobs,
                 total_jobs, perc);
  }

  if (perc == 100) {
    printJobSummary();
  }
}

void ParallaxGenTask::printJobSummary() {
  // Print each job status result
  string output_log = task_name + " Summary: ";
  for (const auto &pair : num_jobs_completed) {
    string state_str = pg_result_str[pair.first];
    output_log += "[ " + state_str + " : " + to_string(pair.second) + " ] ";
  }
  output_log += "See log to see error messages, if any.";
  spdlog::info(output_log);
}

size_t ParallaxGenTask::getCompletedJobs() {
  // Initialize the sum variable
  size_t sum = 0;

  // Iterate through the unordered_map and sum the values
  for (const auto &pair : num_jobs_completed) {
    sum += pair.second;
  }

  return sum;
}

void ParallaxGenTask::updatePGResult(PGResult &result,
                                     const PGResult &current_result,
                                     const PGResult &threshold) {
  if (current_result > result) {
    if (current_result > threshold) {
      result = threshold;
    } else {
      result = current_result;
    }
  }
}
