#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <exception>
#include <initializer_list>

#define failmsg std::cout << "\e[38;5;202m" << "[FAIL][" << __FILE__ << ':' << std::to_string(__LINE__) << "]\e[0m "
#define infomsg std::cout << "\e[38;5;42m" << "[INFO][" << __FILE__ << ':' << std::to_string(__LINE__) << "]\e[0m "

namespace test {
struct Suite {
  std::string name;
  using Predicate = std::vector<std::function<bool()> >;
  Predicate tests;
  Suite(std::string name, Predicate tests) : name(name), tests(tests) { }
};


extern std::vector<Suite> MasterSuite;
extern Suite StringExpanderSuite;
};

