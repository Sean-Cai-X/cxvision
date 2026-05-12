/*
  File: control_flow_regression_main.cpp
  Role: Executable entry point for smoke or regression validation.
*/

#include "muParserTest.h"

#include <exception>
#include <iostream>

/*
  Role: Run the control-flow regression profile as a standalone executable.
*/
int main()
{
  try
  {
    mu::Test::ParserTester tester(mu::Test::ParserTester::ControlFlowProfile);
    tester.SetStream(&std::cout);
    tester.Run();
    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 2;
  }
  catch (...)
  {
    std::cerr << "[EXCEPTION] unknown" << std::endl;
    return 3;
  }
}
