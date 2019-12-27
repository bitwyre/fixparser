#include "fixparser.hpp"
#include <iostream>

auto main() -> int {

  std::string msg("8=FIX.4.4|9=147|35=F|34=3|49=TRADEBOTOE002|52=20180425-17:57:59.000|56=GEMINI|11=GHDzdNUUXaMMDZdfwe|38=1|41=z35u64KR1gen7f2SpB|54=2|55=BTCUSD|60=20180425-17:57:59|10=185|");

  fixparser::Config cfg;

  std::cout << fixparser::checkMsgValidity(msg, cfg) << "\n";
  std::cout << fixparser::getErrors() << "\n";
  return 1;
}
