
#ifndef MAIN_H
#define MAIN_H
 
#ifdef __cplusplus
extern "C" {
#endif 
    int glfw_occ_main();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

struct CxScriptHeadlessOptions;
struct CxScriptHeadlessResult;

bool ParseCxScriptHeadlessArgs(int argc, char** argv, CxScriptHeadlessOptions& options);
#endif

#endif
