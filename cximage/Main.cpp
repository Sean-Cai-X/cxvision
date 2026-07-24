
#include "ViewController.h"
#include "Main.h"
int glfw_occ_main ( )
{
  ViewController anApp;
  try
  {
    anApp.run();
  }
  catch (const std::runtime_error& theError)
  {
    std::cerr << theError.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
