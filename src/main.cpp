#include "fixparser.hpp"
#include <iostream>

auto main() -> int {

  std::string msg("8=FIX.4.4|9=114|35=V|34=2|49=TRADEBOTMD002|52=20180425-17:51:40.000|56=BITWYRE|262=2|263=1|264=1|265=0|146=1|55=BTCUSD|267=1|269=0|10=016|");

  fixparser::checkMsgValidity(msg);

  return 1;
}