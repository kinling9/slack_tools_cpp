#pragma once
#include <absl/container/flat_hash_map.h>

#include <string>

class design_cons {
 public:
  // Public method to get the instance of the design_cons
  static design_cons& get_instance();

  // Delete the methods we don't want
  design_cons(const design_cons&) = delete;  // Prevent copy-construction
  design_cons& operator=(const design_cons&) = delete;  // Prevent assignment

  // Public method for demonstration purposes
  double get_period(const std::string& design_name);
  std::string get_name(const std::string& rpt_name);

 private:
  // Private constructor to prevent instancing
  design_cons();

  // Destructor
  ~design_cons();

  absl::flat_hash_map<std::string, double> _period;
};
