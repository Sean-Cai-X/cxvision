cmake_minimum_required(VERSION 3.21)

project(cxvision_imgui_acceptance LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GLFW_ROOT "D:/glfw-3.3.10" CACHE PATH "Path to the GLFW source tree")
set(GLAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../analysis_workspace/imGuIZMO.quat/libs/glad" CACHE PATH "Path to glad sources")
set(OCCT_ROOT "D:/OpenCASCADE-7.7.0-vc14-64/opencascade-7.7.0" CACHE PATH "Path to OpenCASCADE")
set(OpenCV_DIR "D:/opencv/build" CACHE PATH "Path to OpenCVConfig.cmake")

if(NOT EXISTS "${GLFW_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "GLFW_ROOT is invalid: ${GLFW_ROOT}")
endif()
if(NOT EXISTS "${GLAD_ROOT}/glad.cpp")
  message(FATAL_ERROR "GLAD_ROOT is invalid: ${GLAD_ROOT}")
endif()
if(NOT EXISTS "${OCCT_ROOT}/inc/Standard.hxx")
  message(FATAL_ERROR "OCCT_ROOT is invalid: ${OCCT_ROOT}")
endif()

find_package(OpenCV REQUIRED CONFIG)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLFW_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw")

set(CXIMAGE_SOURCES
  cximage/GuiMain.cpp
  cximage/ManualStateTestConsole.cpp
  cximage/ParserDebugBridge.cpp
  cximage/ImageAnnotationLayer.cpp
  cximage/ImageAnnotationUI.cpp
  cximage/Main.cpp
  cximage/ViewController.cpp
  cximage/SemanticFlowGraph.cpp
  cximage/Window.cpp
  cximage/View.cpp
  cximage/Image.cpp
  cximage/imagemanager.cpp
  cximage/Shape.cpp
  cximage/shapebase.cpp
  cximage/gp_path.cpp
  cximage/Grid.cpp
  cximage/Findline.cpp
  cximage/Findcircle.cpp
  cximage/Findellipse.cpp
  cximage/findobject.cpp
  cximage/FastMatch.cpp
  cximage/Run.cpp
  cximage/ParserClass.cpp
)

set(CXGEOM_SOURCES
  cxgeom/src/CxShapeHandle.cpp
  cxgeom/src/CxCurveBuilder.cpp
  cxgeom/src/CxSetCircleBuild.cpp
  cxgeom/src/CxSetLineBuild.cpp
)

set(CXPARSER_SOURCES
  cxparser/muParser.cpp
  cxparser/muParserBase.cpp
  cxparser/muParserBytecode.cpp
  cxparser/muParserCallback.cpp
  cxparser/muParserClass.cpp
  cxparser/muParserClassFunctionReader.cpp
  cxparser/muparserClassReader.cpp
  cxparser/muParserError.cpp
  cxparser/muParserInt.cpp
  cxparser/muParserRun.cpp
  cxparser/muParserStrClassMap.cpp
  cxparser/muParserTest.cpp
  cxparser/muparsertokenreader.cpp
  cxparser/muParserTreeNode.cpp
)

set(IMGUI_SOURCES
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_impl_glfw.cpp
  imgui/imgui_impl_opengl3.cpp
)

add_executable(cxvision_imgui_acceptance
  ${CXIMAGE_SOURCES}
  ${CXGEOM_SOURCES}
  ${CXPARSER_SOURCES}
  ${IMGUI_SOURCES}
  "${GLAD_ROOT}/glad.cpp"
)

target_include_directories(cxvision_imgui_acceptance PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/cximage"
  "${CMAKE_CURRENT_SOURCE_DIR}/cxparser"
  "${CMAKE_CURRENT_SOURCE_DIR}/cxgeom/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
  "${GLAD_ROOT}/.."
  "${OCCT_ROOT}/inc"
  ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(cxvision_imgui_acceptance PRIVATE
  "${OCCT_ROOT}/win64/vc14/lib"
)

target_compile_definitions(cxvision_imgui_acceptance PRIVATE
  _CRT_SECURE_NO_WARNINGS
  CXCORE_ENABLE_VIEWCONTROLLER_CUDA=0
  ImTextureID=ImU64
)

target_link_libraries(cxvision_imgui_acceptance PRIVATE
  glfw
  opengl32
  TKernel
  TKMath
  TKG2d
  TKG3d
  TKService
  TKV3d
  TKOpenGl
  TKGeomBase
  TKBRep
  TKGeomAlgo
  TKTopAlgo
  TKPrim
  TKBO
  TKOffset
  TKXSBase
  TKSTEPBase
  TKIGES
  TKLCAF
  ${OpenCV_LIBS}
)

if(MSVC)
  target_compile_options(cxvision_imgui_acceptance PRIVATE /EHsc /W3)
endif()

set_target_properties(cxvision_imgui_acceptance PROPERTIES
  OUTPUT_NAME "cxvision_imgui_acceptance"
  RUNTIME_OUTPUT_DIRECTORY "D:/Codex-WorkDir/Sean_WorkDir/cxparser/build"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "D:/Codex-WorkDir/Sean_WorkDir/cxparser/build/Release"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "D:/Codex-WorkDir/Sean_WorkDir/cxparser/build/Debug"
)

add_custom_command(TARGET cxvision_imgui_acceptance POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${OCCT_ROOT}/win64/vc14/bin"
          "$<TARGET_FILE_DIR:cxvision_imgui_acceptance>"
  COMMENT "Copy matching OpenCASCADE runtime DLLs"
)