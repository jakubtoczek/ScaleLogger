#include "core/KeySequence.hpp"

#include <cassert>

using namespace scalelogger;

int RunKeySequenceTests() {
  auto t1 = NormalizeKeyToken(" Enter ");
  assert(t1.has_value() && *t1 == "enter");
  auto t2 = NormalizeKeyToken("tab");
  assert(t2.has_value() && *t2 == "tab");
  auto t3 = NormalizeKeyToken("bad_key");
  assert(!t3.has_value());
  auto t4 = NormalizeKeyToken("F12");
  assert(!t4.has_value());
  auto t5 = NormalizeKeyToken("Num6");
  assert(!t5.has_value());
  auto t6 = NormalizeKeyToken("a");
  assert(!t6.has_value());
  auto seq = ParseSequenceCsv("down, right, tab, enter");
  assert(seq.size() == 4);
  auto seq2 = ParseSequenceCsv("up, left, esc, space");
  assert(seq2.size() == 4);
  assert(seq2[0] == "up");
  assert(seq2[1] == "left");
  assert(seq2[2] == "esc");
  assert(seq2[3] == "space");
  auto seq3 = ParseSequenceCsv("down, f10, right, num1");
  assert(seq3.size() == 2);
  assert(seq3[0] == "down");
  assert(seq3[1] == "right");
  return 0;
}
