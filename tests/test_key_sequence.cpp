#include "core/KeySequence.hpp"

#include <cassert>

using namespace scalelogger;

int RunKeySequenceTests() {
  auto t1 = NormalizeKeyToken(" Enter ");
  assert(t1.has_value() && *t1 == "enter");
  auto t2 = NormalizeKeyToken("bad_key");
  assert(!t2.has_value());
  auto seq = ParseSequenceCsv("down, right, tab");
  assert(seq.size() == 3);
  return 0;
}