/**
 * \file src/exec_manager/execution.cc
 * \brief Contains the \c Algorithm class methods.
 *
 * \authors Rodrigo Alves Prado da Silva \<rodrigo_prado@id.uff.br\>
 * \copyright Fluminense Federal University (UFF)
 * \copyright Computer Science Department
 * \date 2020
 *
 * This source file contains the methods from the \c Algorithm class that run the mode the
 * approximate solution.
 */

#include "src/solution/algorithm.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/bimap.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include "src/model/file.h"
#include "src/model/static_file.h"
#include "src/model/dynamic_file.h"
#include "src/model/requirement.h"
#include "src/model/task.h"
#include "src/model/bucket.h"
#include "src/model/virtual_machine.h"
#include "src/solution/greedy_algorithm.h"
#include "src/solution/min_min_algorithm.h"

void Algorithm::ReadTasksAndFiles(std::string tasks_and_files) {
  double total_file = 0.0;
  // DLOG(INFO) << "total_file: " << total_file;

  // Reading file
  std::string line;
  std::ifstream in_file(tasks_and_files);

  // Get number of tasks and number of files
  getline(in_file, line);
  DLOG(INFO) << "Head: " << line;
  std::vector<std::string> tokens;
  boost::split(tokens, line, boost::is_any_of(" "));

  static_file_size_ = stoul(tokens[0]);
  dynamic_file_size_ = stoul(tokens[1]);
  task_size_ = stoul(tokens[2]) + 2;  // Adding two tasks (source and target)
  requirement_size_ = stoul(tokens[3]);
  makespan_max_ = stod(tokens[4]);
  budget_max_ = stod(tokens[5]);
  file_size_ = static_file_size_ + dynamic_file_size_;
  tasks_plus_files_size_ = task_size_ + file_size_;  // Tasks + Files

  // DLOG(INFO) << "static_file_size_: " << static_file_size_;
  // DLOG(INFO) << "dynamic_file_size_: " << dynamic_file_size_;
  // DLOG(INFO) << "task_size_: " << task_size_;
  // DLOG(INFO) << "requirement_size_: " << requirement_size_;
  // DLOG(INFO) << "makespan_max_: " << makespan_max_;
  // DLOG(INFO) << "budget_max_: " << budget_max_;
  // DLOG(INFO) << "file_size_: " << file_size_;
  // DLOG(INFO) << "tasks_plus_files_size_: " << tasks_plus_files_size_;

  getline(in_file, line);  // Reading blank line

  // Start initial integer_id of elements
  // Tasks, dynamic files, and static files do share the same range of IDs
  size_t initial_dynamic_file_id = task_size_;
  size_t initial_static_file_id = task_size_ + dynamic_file_size_;

  // Reading information about requirements
  for (size_t i = 0; i < requirement_size_; i++) {
    getline(in_file, line);
    DLOG(INFO) << "Requirement: " << line;
    google::FlushLogFiles(google::INFO);

    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of(" "));

    size_t id = stoul(strs[0]);
    double max_value = stod(strs[1]);
    Requirement my_requirement = Requirement(id, max_value);
    requirements_.push_back(my_requirement);
    DLOG(INFO) << my_requirement;
  }

  getline(in_file, line);  // reading blank line

  file_map_per_id_.reserve(file_size_);
  file_map_per_name_.reserve(file_size_);

  // Reading information about static files
  for (size_t i = initial_static_file_id; i < initial_static_file_id + static_file_size_; i++) {
    getline(in_file, line);
    DLOG(INFO) << "File: " << line;
    google::FlushLogFiles(google::INFO);

    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of(" "));
    auto file_name = strs[0];
    auto file_size = stod(strs[1]);

    // DLOG(INFO) << "file_name: " << file_name;
    // DLOG(INFO) << "file_size: " << file_size;

    StaticFile* my_staticFile = new StaticFile(i, file_name, file_size);

    for (size_t j = 0; j < stoul(strs[2]); ++j) {
      my_staticFile->AddVm(stoul(strs[3 + j]));
    }

    total_file += file_size;

    DLOG(INFO) << *my_staticFile;

    file_map_per_id_.insert(std::make_pair(i, my_staticFile));
    file_map_per_name_.insert(std::make_pair(file_name, my_staticFile));
  }

  // Reading information about dynamic files
  for (size_t i = initial_dynamic_file_id; i < initial_dynamic_file_id + dynamic_file_size_; i++) {
    getline(in_file, line);
    DLOG(INFO) << "File: " << line;
    google::FlushLogFiles(google::INFO);

    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of(" "));
    const std::string file_name = strs[0];
    auto file_size = stod(strs[1]);

    // DLOG(INFO) << "file_name: " << file_name;
    // DLOG(INFO) << "file_size: " << file_size;

    // DynamicFile my_dynamicFile(i, file_name, file_size);
    DynamicFile* my_dynamicFile = new DynamicFile(i, file_name, file_size);

    total_file += file_size;

    DLOG(INFO) << *my_dynamicFile;

    file_map_per_id_.insert(std::make_pair(i, my_dynamicFile));
    file_map_per_name_.insert(std::make_pair(file_name, my_dynamicFile));
  }

  getline(in_file, line);  // reading blank line

  task_map_per_id_.reserve(task_size_);
  task_map_per_name_.reserve(task_size_);

  // Reading information about tasks
  for (size_t i = 1; i < task_size_ - 1; i++) {
    getline(in_file, line);
    DLOG(INFO) << "Task: " << line;
    google::FlushLogFiles(google::INFO);
    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of(" "));
    // get task info
    auto tag = strs[0];
    std::string task_name = strs[1];
    auto base_time = stod(strs[2]);
    auto in_size = stoul(strs[3]);
    auto out_size = stoul(strs[4]);

    // DLOG(INFO) << "tag: " << tag;
    // DLOG(INFO) << "task_name: " << task_name;
    // DLOG(INFO) << "base_time: " << base_time;
    // DLOG(INFO) << "in_size: " << in_size;
    // DLOG(INFO) << "out_size: " << out_size;

    Task my_task(i, tag, task_name, base_time);

    // Adding requirement to the task
    for (size_t j = 5ul; j < 5ul + requirement_size_; ++j) {
      auto requirement_value = stod(strs[j]);
      my_task.AddRequirement(requirement_value);
    }

    // reading input files
    for (size_t j = 0; j < in_size; j++) {
      getline(in_file, line);
      DLOG(INFO) << "Input file: " << line;
      google::FlushLogFiles(google::INFO);

      File* my_file = file_map_per_name_.find(line)->second;
      my_task.AddInputFile(my_file);
    }

    // reading output files
    for (size_t j = 0; j < out_size; j++) {
      getline(in_file, line);
      DLOG(INFO) << "Output file: " << line;
      google::FlushLogFiles(google::INFO);

      File* my_file = file_map_per_name_.find(line)->second;
      my_task.AddOutputFile(my_file);
    }

    task_map_per_id_.insert(std::make_pair(i, my_task));
    task_map_per_name_.insert(std::make_pair(tag, my_task));

    DLOG(INFO) << my_task;
  }  // for (size_t i = 1; i < task_size_ - 1; i++) {

  DLOG(INFO) << "task_map_per_id_: ";
  for (auto task : task_map_per_name_) {
    DLOG(INFO) << "ID: " << task.first << ", " << task.second;
  }

  DLOG(INFO) << "task_map_per_name_: ";
  for (auto task : task_map_per_name_) {
    DLOG(INFO) << "Task: " << task.second;
  }

  getline(in_file, line);  // reading blank line

  // Update Source and Target tasks
  id_source_ = 0;
  // id_target_ = initial_task_id;
  id_target_ = task_size_ - 1;
  // initial_task_id += 1;

  // key_map.insert(make_pair("root", id_source));
  // key_map.insert(make_pair("sink", id_target));

  Task source_task(id_source_, "source", "SOURCE", 0.0);
  Task target_task(id_target_, "target", "TARGET", 0.0);

  for (size_t i = 0; i < requirement_size_; ++i) {
    source_task.AddRequirement(0);
    target_task.AddRequirement(0);
  }

  task_map_per_id_.insert(std::make_pair(id_source_, source_task));
  task_map_per_id_.insert(std::make_pair(id_target_, target_task));

  task_map_per_name_.insert(std::make_pair("source", source_task));
  task_map_per_name_.insert(std::make_pair("target", target_task));

  // for (auto pair : task_map_per_name_) {
  //   DLOG(INFO) << "task_map_[" << pair.first << ", " << pair.second << "]";
  //   // google::FlushLogFiles(google::INFO);

  // }

  auto f_source = successors_.insert(std::make_pair(id_source_, std::vector<size_t>()));
  successors_.insert(std::make_pair(id_target_, std::vector<size_t>()));

  std::vector<int> aux(task_size_, -1);
  aux[id_source_] = 0;
  aux[id_target_] = 0;

  // Reading successors graph informations
  for (size_t i = 0; i < task_size_ - 2; i++) {
    getline(in_file, line);  // Reading parent task

    DLOG(INFO) << "Parent: " << line;
    google::FlushLogFiles(google::INFO);

    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of(" "));
    auto task_tag = strs[0];
    auto number_of_successors = stoi(strs[1]);

    std::vector<size_t> children;
    // Reading children task
    for (int j = 0; j < number_of_successors; j++) {
      getline(in_file, line);

      DLOG(INFO) << "Child: " << line;
      google::FlushLogFiles(google::INFO);

      // auto child_id = key_map.find(line)->second;
      auto child_task = task_map_per_name_.find(line)->second;

      children.push_back(child_task.get_id());
      aux[child_task.get_id()] = 0;
    }

    // Target task
    if (number_of_successors == 0) {
      children.push_back(id_target_);
    }

    // auto task_id = key_map.find(task_tag)->second;
    auto task = task_map_per_name_.find(task_tag)->second;

    successors_.insert(make_pair(task.get_id(), children));
  }

  // Add synthetic source task
  for (size_t i = 0; i < task_size_; i++) {
    // Add source
    if (aux[i] == -1) {
      f_source.first->second.push_back(i);
    }
  }

  predecessors_ = ReverseMap(successors_);

  in_file.close();
}  // void Algorithm::ReadTasksAndFiles(std::string tasks_and_files) {

void Algorithm::ReadCluster(std::string cluster) {
  double total_storage = 0.0;
  // DLOG(INFO) << "total_storage: " << total_storage;

  // Reading file
  std::string line;
  std::ifstream in_cluster(cluster);
  std::vector<std::string> tokens;

  getline(in_cluster, line);

  DLOG(INFO) << "Head: " << line;
  google::FlushLogFiles(google::INFO);

  boost::split(tokens, line, boost::is_any_of(" "));

  // size_t number_of_providers = stoul(tokens[0]);
  size_t number_of_requirements = stoul(tokens[1]);

  getline(in_cluster, line);  // ignore line

  // size_t provider_id = 0ul;
  size_t storage_id = 0ul;
  // size_t bucket_id = 0ul;

  // for (size_t i = 0; i < number_of_providers_; ++i) {
    getline(in_cluster, line);
    DLOG(INFO) << "Provider: " << line;
    google::FlushLogFiles(google::INFO);

    std::vector<std::string> strs1;
    boost::split(strs1, line, boost::is_any_of(" "));

    period_hr_ = stod(strs1[2]);
    vm_size_ = stoul(strs1[4]);
    bucket_size_ = stoul(strs1[5]);

    // Reading VMs information
    for (auto j = 0ul; j < vm_size_; j++) {
      getline(in_cluster, line);
      DLOG(INFO) << "VM: " << line;
      google::FlushLogFiles(google::INFO);

      std::vector<std::string> strs;
      boost::split(strs, line, boost::is_any_of(" "));

      int type_id = stoi(strs[0]);
      std::string vm_name = strs[1];
      double slowdown = stod(strs[2]);
      double storage = stod(strs[3]) * 1024;  // GB to MB
      double bandwidth = stod(strs[4]);
      double cost = stod(strs[5]);

      VirtualMachine my_vm(storage_id, vm_name, slowdown, storage, cost, bandwidth, type_id);

      // Adding requirement to the task
      for (size_t l = 6ul; l < 6ul + number_of_requirements; ++l) {
        auto requirement_value = stod(strs[l]);
        my_vm.AddRequirement(requirement_value);
      }

      vm_map_.insert(std::make_pair(storage_id, my_vm));
      storage_map_.insert(std::make_pair(storage_id, my_vm));
      storage_id += 1;
      total_storage += storage;
      DLOG(INFO) << my_vm;
    }

    // Reading Buckets informations
    for (auto j = 0ul; j < bucket_size_; j++) {
      getline(in_cluster, line);
      DLOG(INFO) << "Bucket: " << line;
      google::FlushLogFiles(google::INFO);

      std::vector<std::string> strs;
      boost::split(strs, line, boost::is_any_of(" "));

      int type_id = stoi(strs[0]);
      std::string name = strs[1];
      double storage = stod(strs[2]) * 1024;  // GB to MB
      double bandwidth = stod(strs[3]);
      size_t number_of_intervals = 1ul;
      double cost = stod(strs[4]);

      Bucket my_bucket(storage_id, name, storage, cost, bandwidth, type_id, number_of_intervals);

      // Adding requirement to the task
      for (size_t l = 5ul; l < 5ul + number_of_requirements; ++l) {
        auto requirement_value = stod(strs[l]);
        my_bucket.AddRequirement(requirement_value);
      }

      storage_map_.insert(std::make_pair(storage_id, my_bucket));
      storage_id += 1;
      total_storage += storage;
      DLOG(INFO) << my_bucket;
      google::FlushLogFiles(google::INFO);
    }
    // providers_.push_back(my_provider);
  // }
  in_cluster.close();
}  // void Algorithm::ReadCluster(std::string cluster) {

void Algorithm::ReadConflictGraph(std::string conflict_graph) {
  std::string line;
  std::vector<std::string> tokens;

  std::ifstream in_conflict_graph(conflict_graph);
  // conflict_graph_.redefine(size_, size_, 0.0);

  // conflict_graph_ = ConflictGraph(size_);
  conflict_graph_.Redefine(tasks_plus_files_size_);

  // Reading conflict graph informations
  while (getline(in_conflict_graph, line)) {
    DLOG(INFO) << "Conflict: " << line;
    google::FlushLogFiles(google::INFO);

    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of(" "));
    auto first_file = strs[0];
    auto second_file = strs[1];
    auto conflict_value = stod(strs[2]);
    auto first_file_id = file_map_per_name_.find(first_file)->second->get_id();
    auto second_file_id = file_map_per_name_.find(second_file)->second->get_id();
    // DLOG(INFO) << "firstFile: " << firstFile;
    // DLOG(INFO) << "secondFile: " << secondFile;
    // DLOG(INFO) << "conflictValue: " << conflictValue;
    // DLOG(INFO) << "firstId: " << firstId;
    // DLOG(INFO) << "secondId: " << secondId;
    // google::FlushLogFiles(google::INFO);
    // conflict_graph_(firstId, secondId) = conflictValue;
    // conflict_graph_(secondId, firstId) = conflictValue;
    conflict_graph_.AddConflict(first_file_id, second_file_id, conflict_value);
  }

  // DLOG(INFO) << "Conflict Graph: " << conflict_graph_;

  in_conflict_graph.close();
}

/**
 * The \c RunSetup() method calls \c DigitalRock methods involved in loading the greyscale cube and
 * calculating the greyscale histogram for later segmentation.
 *
 * \param[in] json_file_name  Name of JSON file containing setup-related parameters.
 */
void Algorithm::ReadInputFiles(const std::string tasks_and_files,
                               const std::string cluster,
                               const std::string conflict_graph) {
  ReadTasksAndFiles(tasks_and_files);
  ReadCluster(cluster);
  ReadConflictGraph(conflict_graph);
  storage_vet_.resize(storage_map_.size(), 0.0);
  for (auto storage_pair : storage_map_) {
    Storage storage = storage_pair.second;
    storage_vet_[storage.get_id()] = storage.get_storage();
  }
  height_.resize(task_size_, -1);
  ComputeHeight(id_source_, 0);
  for (size_t i = 0; i < height_.size(); ++i) {
    DLOG(INFO) << "Height[" << i << "]: " << height_[i];
  }
  // google::FlushLogFiles(google::INFO);
  // Check if storage is enough
  // for (auto it : _file_map){
  //   // if (it.second.is_static){
  //   if (it.second.isStatic()) {
  //     // _storage_vet[it.second.getFirstVm()] -= it.second.size;
  //     StaticFile &sf = static_cast<StaticFile&>(it.second);
  //     _storage_vet[sf.getFirstVm()]
  //       -= it.second.getSize();
  //     if (_storage_vet[sf.getFirstVm()] < 0) {
  //       std::cerr << "Static file is bigger than the vm capacity" << std::endl;
  //       throw;
  //     }
  //   }
  // }

  // if (total_storage < total_file) {
  //   std::cerr << "Storage is not enough" << std::endl;
  //   throw;
  // }
}  // end of Algorithm::ReadInputFiles method

/**
 * The \c getAlgorithm() returns an object derived from \c Algorithm depending on the
 * \c name parameter.
 *
 * \param[in] name      Name of the \c Algorithm object to be instantiated.
 * \retval    algorithm Instance of an \c Algorithm object.
 */
std::shared_ptr<Algorithm> Algorithm::ReturnAlgorithm(const std::string algorithm) {
  if (algorithm == "greedy") {
    return std::make_shared<GreedyAlgorithm>();
  } else if (algorithm == "min_min") {
    return std::make_shared<MinMinAlgorithm>();
  } else {
    std::fprintf(stderr, "Please select a valid algorithm.\n");
    std::exit(-1);
  }
}  // end of Algorithm::GetAlgorithm() method

std::unordered_map<size_t, std::vector<size_t>> Algorithm::ReverseMap(
    std::unordered_map<size_t, std::vector<size_t>> amap) {

  std::unordered_map<size_t, std::vector<size_t>> r_map;

  for (auto key : amap) {
    for (auto val : amap.find(key.first)->second) {
      auto f = r_map.insert(std::make_pair(val, std::vector<size_t>(0)));
      f.first->second.push_back(key.first);
    }
  }

  return r_map;
}

void Algorithm::ComputeHeight(size_t node, int n) {
  // DLOG(INFO) << "node[" << node << "], n[" << n << "]";
  // google::FlushLogFiles(google::INFO);
  if (height_[node] < n) {
    height_[node] = n;
    auto vet = successors_.find(node)->second;
    for (auto j : vet) {
      // DLOG(INFO) << "vet[" << j << "]";
      // google::FlushLogFiles(google::INFO);

      ComputeHeight(j, n+1);
    }
  }
}

// void Algorithm::ComputeHeight(int node, int n) {
//   if (height[node] < n) {
//     height[node] = n;
//     auto vet = succ_.find(node)->second;
//     for (auto j : vet) {
//       ComputeHeight(j, n+1);
//     }
//   }
// }

Algorithm::~Algorithm() {
  DLOG(INFO) << "~Algorithm";
  google::FlushLogFiles(google::INFO);

  for (std::pair<size_t, File*> p : file_map_per_id_) {
    delete p.second;
  }

  DLOG(INFO) << "deleting done";
  google::FlushLogFiles(google::INFO);
}

void Algorithm::CalculateMaximumSecurityAndPrivacyExposure() {
  double maximum_task_exposure = 0.0;

  DLOG(INFO) << "Calculate the Maximum Security and Privace Exposure";

  for (Requirement requirement : requirements_) {
    maximum_task_exposure += static_cast<double>(task_size_)
                   * static_cast<double>(requirement.get_max_value());
  }

  DLOG(INFO) << "task_exposure: " << maximum_task_exposure;

  double maximum_privacy_exposure =
      static_cast<double>(conflict_graph_.get_maximum_of_soft_constraints());

  maximum_security_and_privacy_exposure_ = maximum_task_exposure + maximum_privacy_exposure;

  DLOG(INFO) << "maximum_security_and_privacy_exposure_: "
      << maximum_security_and_privacy_exposure_;
}  // void Algorithm::CalculateSecurityExposure() {
