#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#define FULL_PERCENTAGE 100

class ParallaxGenTask {
public:
  enum class PGResult { SUCCESS, SUCCESS_WITH_WARNINGS, FAILURE };

private:
  int ProgressPrintModulo;

  std::string TaskName;
  size_t TotalJobs;
  size_t LastPerc = 0;
  std::mutex NumJobsCompletedMutex;

  std::unordered_map<PGResult, size_t> NumJobsCompleted;

  std::unordered_map<PGResult, std::string> PGResultStr = {{PGResult::SUCCESS, "COMPLETED"},
                                                           {PGResult::SUCCESS_WITH_WARNINGS, "COMPLETED WITH WARNINGS"},
                                                           {PGResult::FAILURE, "FAILED"}};

public:
  ParallaxGenTask(std::string TaskName, const size_t &TotalJobs, const int &ProgressPrintModulo = 1);

  void completeJob(const PGResult &Result);
  void initJobStatus();
  void printJobStatus(bool Force = false);
  void printJobSummary();
  [[nodiscard]] auto getCompletedJobs() -> size_t;
  [[nodiscard]] auto isCompleted() -> bool;

  static void updatePGResult(PGResult &Result, const PGResult &CurrentResult,
                             const PGResult &Threshold = PGResult::FAILURE);
};
