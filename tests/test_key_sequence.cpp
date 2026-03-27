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
  auto t4 = NormalizeKeyToken("Num6");
  assert(t4.has_value() && *t4 == "num6");
  auto t5 = NormalizeKeyToken("num6");
  assert(t5.has_value() && *t5 == "num6");
  auto t6 = NormalizeKeyToken("Numpad2");
  assert(t6.has_value() && *t6 == "num2");
  auto t7 = NormalizeKeyToken("  Num2  ");
  assert(t7.has_value() && *t7 == "num2");
  auto seq = ParseSequenceCsv("down, right, tab, f10");
  assert(seq.size() == 4);
  auto seq2 = ParseSequenceCsv("Num6, numpad2, Numpad0");
  assert(seq2.size() == 3);
  assert(seq2[0] == "num6");
  assert(seq2[1] == "num2");
  assert(seq2[2] == "num0");
  return 0;
}
