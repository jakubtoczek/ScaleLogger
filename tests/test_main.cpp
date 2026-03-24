#include <exception>
#include <iostream>

int RunParserTests();
int RunConfigTests();
int RunKeySequenceTests();

int main() {
  try {
    if (RunParserTests() != 0) return 1;
    if (RunConfigTests() != 0) return 1;
    if (RunKeySequenceTests() != 0) return 1;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
