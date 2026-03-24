#include "core/KeySequence.hpp"

#include <cassert>

using namespace scalelogger;

int RunKeySequenceTests() {
  auto t1 = NormalizeKeyToken(" Enter ");
  assert(t1.has_value() && *t1 == "enter");
  auto t2 = NormalizeKeyToken("F12");
  assert(t2.has_value() && *t2 == "f12");
  auto t3 = NormalizeKeyToken("bad_key");
  assert(!t3.has_value());
  auto seq = ParseSequenceCsv("down, right, tab, f10");
  assert(seq.size() == 4);
  return 0;
}