/**
 * \file src/solution/algorithm.h
 * \brief Contains the \c Algorithm class declaration.
 *
 * \authors Rodrigo Alves Prado da Silva \<rodrigo_prado@id.uff.br\>
 * \copyright Fluminense Federal University (UFF)
 * \copyright Computer Science Department
 * \date 2020
 *
 * This header file contains the \c Algorithm class that handles different execution modes.
 */

#ifndef APPROXIMATIVE_SOLUTIONS_SRC_SOLUTION_ALGORITHM_H_
#define APPROXIMATIVE_SOLUTIONS_SRC_SOLUTION_ALGORITHM_H_

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include "src/model/file.h"
#include "src/model/requirement.h"
#include "src/model/task.h"
#include "src/model/virtual_machine.h"
#include "src/model/bucket.h"
#include "src/model/provider.h"
#include "src/model/solution.h"
#include "src/model/conflict_graph.h"

#include "src/data_structure/matrix.h"

class Solution;

/**
 * \class Algorithm algorithm.h "src/solution/algorithm.h"
 * \brief Executes the appropriate routines.
 *
 * This class is responsible for calling the appropriate methods for read the input files and
 * execute a specific implementation.
 */

class Algorithm {
 public:
  Algorithm() = default;

  virtual ~Algorithm();

  /// Read input files.
  void ReadInputFiles(const std::string tasks_and_files,
                      const std::string cluster,
                      const std::string conflict_graph);

  /// Getter for \c id_source_
  size_t get_id_source() const { return id_source_; }

  /// Getter for \c id_target_
  size_t get_id_target() const { return id_target_; }

  /// Getter for \c conflict_graph_
  ConflictGraph& get_conflict_graph() { return conflict_graph_; }

  /// Getter for \c storage_vet_
  std::vector<double>& get_storage_vet() { return storage_vet_; }

  /// Getter for \c height_
  std::vector<int>& get_height() { return height_; }

  /// Getter for \c bucket_size_
  size_t get_bucket_size() { return bucket_size_; }

  /// Return the size of the \c tasks_
  size_t GetTaskSize() const { return tasks_.size(); }

  /// Return the size of the \c files_
  size_t GetFileSize() const { return files_.size(); }

  /// Return the size of the \c virtual_machines
  size_t GetVirtualMachineSize() const { return virtual_machines_.size(); }

  /// Return the size of the \c storages_
  size_t GetStorageSize() const { return storages_.size(); }

  /// Return the size of the \c requirements_
  size_t GetRequirementsSize() const { return requirements_.size(); }

  /// Return a pointer to the \c File identified by \c id
  File* GetFilePerId(size_t id) { return files_[id]; }

  /// Return a pointer to the \c Task identified by \c id
  Task* GetTaskPerId(size_t id) { return tasks_[id]; }

  /// Return a pointer to the \c Storage identified by \c id
  Storage* GetStoragePerId(size_t id) { return storages_[id]; }

  /// Return a pointer to the \c VirtualMachine identified by \c id
  VirtualMachine* GetVirtualMachinePerId(size_t id) { return virtual_machines_[id]; }

  /// Return a pointer to the \c Requirement identified by \c id
  Requirement GetRequirementPerId(size_t id) { return requirements_[id]; }

  /// Return a reference to the successors of the \c Task identified by \c task_id
  std::vector<size_t>& GetSuccessors(size_t task_id) { return successors_[task_id]; }

  /// Return a reference to the predecessors of the \c Task identified by \c task_id
  std::vector<size_t>& GetPredecessors(size_t task_id) { return predecessors_[task_id]; }

  /// Getter for makespan_max_
  double get_makespan_max() const { return makespan_max_; }

  /// Getter for \c budget_max_
  double get_budget_max() const { return budget_max_; }

  /// Getter for \c alpha_time_
  double get_alpha_time() const { return alpha_time_; }

  /// Getter for \c alpha_budget_
  double get_alpha_budget() const { return alpha_budget_; }

  /// Getter for \c alpha_security_
  double get_alpha_security() const { return alpha_security_; }

  /// Getter for \c maximum_security_and_privacy_exposure_
  double get_maximum_security_and_privacy_exposure() const {
    return maximum_security_and_privacy_exposure_;
  }

  /// Setter for algorithm input parameter initial_time.
  void SetAlphas(double alpha_time,
                 double alpha_budget,
                 double alpha_security,
                 double alpha_restrict_candidate_list) {
    alpha_time_ = alpha_time;
    alpha_budget_ = alpha_budget;
    alpha_security_ = alpha_security;
    alpha_restrict_candidate_list_ = alpha_restrict_candidate_list;
  }

  ///
  void CalculateMaximumSecurityAndPrivacyExposure();

  /**
   * \brief Executes the algorithm.
   */
  virtual void Run() = 0;

  /**
   * \brief Returns an object derived from Algorithm according to \c name parameter.
   */
  static std::shared_ptr<Algorithm> ReturnAlgorithm(const std::string algorithm);

  std::unordered_map<size_t, std::vector<size_t>> ReverseMap(
      std::unordered_map<size_t, std::vector<size_t>> amap);

 protected:
  void ReadTasksAndFiles(std::string, std::unordered_map<std::string, File*>&);

  void ReadCluster(std::string);

  void ReadConflictGraph(std::string, std::unordered_map<std::string, File*>&);

  void ComputeHeight(size_t, int);

  size_t static_file_size_;

  size_t dynamic_file_size_;

  double makespan_max_;

  double budget_max_;

  // size_t tasks_plus_files_size_;

  size_t id_source_;

  size_t id_target_;

  double period_hr_;

  std::vector<Requirement> requirements_;

  std::vector<double> storage_vet_;  // storage of vm

  std::vector<File*> files_;

  std::vector<Task*> tasks_;

  std::vector<Storage*> storages_;

  std::vector<VirtualMachine*> virtual_machines_;

  // Workflow task Graphs
  std::vector<std::vector<size_t>> successors_;

  std::vector<std::vector<size_t>> predecessors_;

  /// Number of the buckets
  size_t bucket_size_ = 0ul;

  std::vector<int> height_;

  ConflictGraph conflict_graph_;

  /// The weight of the time
  double alpha_time_;

  /// The weight of the budget
  double alpha_budget_;

  /// The weight of the security
  double alpha_security_;

  ///
  double alpha_restrict_candidate_list_;

  double maximum_security_and_privacy_exposure_;

  clock_t t_start = clock();
  // double lambda_ =  0.0;  // read and write constant
};  // end of class Algorithm

#endif  // APPROXIMATIVE_SOLUTIONS_SRC_SOLUTION_ALGORITHM_H_
