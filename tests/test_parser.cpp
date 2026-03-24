#include "core/ValueParser.hpp"

#include <cassert>

using namespace scalelogger;

int RunParserTests() {
  ValueParser parser;
  ParsingSettings s;
  auto r1 = parser.Process(" -  0.00123 g ", s);
  assert(r1.ok);
  assert(r1.processed == "-0.00123");
  auto r2 = parser.Process("junk", s);
  assert(!r2.ok);
  s.mode = ParseMode::Raw;
  auto r3 = parser.Process("  any text  ", s);
  assert(r3.ok);
  return 0;
}