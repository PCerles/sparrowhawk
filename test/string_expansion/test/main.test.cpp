#include "Test.test.h"


std::vector <test::Suite> test::MasterSuite {};
int main() {
/* Add your suites here */
  test::MasterSuite.push_back(test::LLVMParserSuite);
  test::MasterSuite.push_back(test::SupportContextSuite);
  test::MasterSuite.push_back(test::GraphSuite);
  test::MasterSuite.push_back(test::End2EndSuite);
  std::cout << "\n\e[48;5;63m[Pipair Tests]\e[0m\n"
          "Suites:\t" << test::MasterSuite.size() << std::endl;
  int failed=0, errors=0, total=0;
  for(auto suite : test::MasterSuite) { 
    total += suite.tests.size();
    std::cout << std::endl << "\e[48;5;26m[ Suite " 
              << suite.name << " ]\e[0m" << std::endl;
    for(auto test : suite.tests) {
      int result;
      try {
        result = test();
        failed += result;
      } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        ++errors;
      }
    }
  }
  std::cout << std::endl << total << " Tests completed [" << 
    "\e[38;5;226m" << errors << " error, " << 
    "\e[38;5;202m" << failed << " failure, " << 
    "\e[38;5;84m" << (total - errors - failed) << " passed\e[0m]\n";
  if(total > 0) {
    int blocks = 50;
    errors = errors * blocks / total;
    failed = failed * blocks / total;
    int passed = (blocks - errors - failed);
    std::cout
      << "\e[48;5;226m" << std::string(errors, ' ')
      << "\e[48;5;202m" << std::string(failed, ' ')
      << "\e[48;5;84m" << std::string(passed, ' ') 
      << "\e[0m" << std::endl;
  }
  std::cout << std::endl;
}
