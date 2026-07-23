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

option(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC
       "Embed cxparser_ext debug layer into cxvision_imgui_acceptance"
       ON)

option(CXVISION_ENABLE_LEGACY_STAGE25_CPP
       "Build deprecated C++ Stage25 manifest/runner implementation"
       OFF)

option(CXVISION_COPY_RUNTIME_DLLS
       "Copy OCCT/OpenCV/LibTorch runtime DLLs next to cxvision_imgui_acceptance after build"
       OFF)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLFW_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw")

set(LIBTORCH_ROOT "D:/libtorch" CACHE PATH "LibTorch root")
add_subdirectory("libtorch_module" "${CMAKE_CURRENT_BINARY_DIR}/_deps/libtorch_module")

set(CXIMAGE_CORE_SOURCES
  cximage/GuiMain.cpp
  cximage/ManualStateTestConsole.cpp
  cximage/CxScriptImageEvidenceAnalyzer.cpp
  cximage/CxScriptGeometryFrameProbe.cpp
  cximage/CxScriptGeometryFrameOverlay.cpp
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
  cximage/FindRect.cpp
  cximage/FindSegmentation.cpp
  cximage/FindSegmentationOpenCvSmokeBackend.cpp
  cximage/FindSegmentationEdgeSamBackend.cpp
  cximage/findobject.cpp
  cximage/FastMatch.cpp
  cximage/FastMatchPolicy.cpp
  cximage/FastMatchDiagnostic.cpp
  cximage/CxFastMatchRuntimeCapture.cpp
  cximage/Run.cpp
  cximage/ParserClass.cpp
  cximage/CxScriptImagePreflight.cpp
  cximage/CircleRingGauge.cpp
  cximage/FormfitFitMethod.cpp
  cximage/FormfitGauge.cpp
  cximage/FormfitPrototype.cpp
  cximage/TorchRuntimeBridge.cpp
  cximage/TorchRuntimeResultAdapter.cpp
  cximage/CxShapeInteractionRunner.cpp
  cximage/CxRuntimeProjectionExecutor.cpp
  cximage/CxShapeInteractionTest.cpp
  cximage/CxShapeTestRegister.cpp
  cximage/CxShapeTestRuntime.cpp
  cximage/CxManifestProjectionRequestResolver.cpp
  cximage/CxParamProbeRunner.cpp
  cximage/CxParserRuntimeOwner.cpp
  cximage/ManualConsoleUtils.cpp
  cximage/ManualConsoleRuntimeView.cpp
  cximage/ManualConsoleCxScriptDebug.cpp
  cximage/ManualConsoleFindcircleDebug.cpp
  cximage/ManualConsoleFindlineDebug.cpp
  cximage/ManualConsoleGauge.cpp
  cximage/ManualConsoleParamRegressionPanel.cpp
  cximage/ManualConsoleEvidenceChain.cpp
  cximage/ManualConsoleScriptDebugPanel.cpp
  cximage/CxScriptCasePackageWriter.cpp
  cximage/CxScriptHeadlessRunner.cpp
  cximage/CxScriptGlobalValueSet.cpp
  cximage/CxScriptHeadlessBindings.cpp
  cximage/CxScriptRuntimeCaptureSmoke.cpp
  cximage/CxUnifiedLog.cpp
  cximage/CxUnifiedLogStreamBuf.cpp
  cximage/CxCrashLogHandler.cpp
  cximage/CxUnifiedLogOptions.cpp
  cximage/CxAnnotationToolRuntime.cpp
  cximage/CxAnnotationToolRegister.cpp
  cximage/LineGaugeShape.cpp
  cximage/RectShape.cpp
  cximage/CircleShape.cpp
  cximage/EllipseShape.cpp
  cximage/PolylineShape.cpp
  cximage/CxEvidenceSelfTestRuntime.cpp
)

set(CXSCRIPT_RUNTIME_SOURCES
  cximage/CxScriptCatalogRuntime.cpp
  cximage/CxScriptCatalogRegister.cpp
  cximage/CxScriptSuiteRuntime.cpp
  cximage/CxScriptSuiteRegister.cpp
  cximage/CxScriptImageManifestRuntime.cpp
  cximage/CxScriptSuiteRunner.cpp
  cximage/CxScriptSuiteReportWriter.cpp
  cximage/CxScriptToolDisplayExporter.cpp
  cximage/CxScriptBestCaseSelector.cpp
  cximage/CxParameterProfileRuntime.cpp
  cximage/CxParameterProfileRegister.cpp
  cximage/CxScriptEvidenceChainRuntime.cpp
  cximage/CxScriptEvidenceChainRegister.cpp
  cximage/CxScriptReviewGateRuntime.cpp
  cximage/CxScriptRunTraceRuntime.cpp
  cximage/CxAlgorithmTraceSink.cpp
  cximage/CxParamRegressionRuntime.cpp
  cximage/CxParamRegressionRegister.cpp
  cximage/CxScriptRuntimeResultCapture.cpp
  cximage/CxShapeOverlayRenderer.cpp
)

set(CXSCRIPT_LEGACY_STAGE25_SOURCES
  cximage/CxScriptStage25Manifest.cpp
  cximage/CxScriptStage25Template.cpp
  cximage/CxScriptStage25Runner.cpp
  cximage/CxScriptStage25ReportWriter.cpp
  cximage/CxScriptStage25Register.cpp
  cximage/CxScriptStage25JsonLite.cpp
  cximage/CxScriptStage25PolicyValidator.cpp
  cximage/CxScriptStage25CaseMatrix.cpp
  cximage/FindlineParameterPolicy.cpp
)

set(CXIMAGE_SOURCES
  ${CXIMAGE_CORE_SOURCES}
  ${CXSCRIPT_RUNTIME_SOURCES}
)

if(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
  list(APPEND CXIMAGE_SOURCES
    ${CXSCRIPT_LEGACY_STAGE25_SOURCES}
  )
  add_compile_definitions(CXVISION_ENABLE_LEGACY_STAGE25_CPP=1)
endif()

set(CXGEOM_SOURCES
  cxgeom/src/CxShapeHandle.cpp
  cxgeom/src/CxCurveBuilder.cpp
  cxgeom/src/CxSetCircleBuild.cpp
  cxgeom/src/CxSetLineBuild.cpp
  cxgeom/src/CxGeomElementBody.cpp
  cxgeom/src/CxGeomAnnotationBody.cpp
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
  cxparser/CxScriptTypeRegistry.cpp
)

set(IMGUI_SOURCES
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_impl_glfw.cpp
  imgui/imgui_impl_opengl3.cpp
)

set(CXPARSER_EXT_DEBUG_SOURCES)
if(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC)
  list(APPEND CXPARSER_EXT_DEBUG_SOURCES
    cxparser_ext/debug/cxscript_debug_embedded_runner.cpp
    cxparser_ext/debug/cxscript_debug_log_builder.cpp
  )
endif()

add_executable(cxvision_imgui_acceptance
  ${CXIMAGE_SOURCES}
  ${CXGEOM_SOURCES}
  ${CXPARSER_SOURCES}
  ${CXPARSER_EXT_DEBUG_SOURCES}
  ${IMGUI_SOURCES}
  "${GLAD_ROOT}/glad.cpp"
)

target_include_directories(cxvision_imgui_acceptance PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/cximage"
  "${CMAKE_CURRENT_SOURCE_DIR}/cxparser"
  "${CMAKE_CURRENT_SOURCE_DIR}/cxparser_ext"
  "${CMAKE_CURRENT_SOURCE_DIR}/cxparser_ext/debug"
  "${CMAKE_CURRENT_SOURCE_DIR}/cxgeom/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
  "${CMAKE_CURRENT_SOURCE_DIR}/libtorch_module"
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


if(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC)
  target_compile_definitions(cxvision_imgui_acceptance PRIVATE
    CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC=1
  )
endif()
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
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
)

if(CXVISION_COPY_RUNTIME_DLLS)
  add_custom_command(TARGET cxvision_imgui_acceptance POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${OCCT_ROOT}/win64/vc14/bin"
            "$<TARGET_FILE_DIR:cxvision_imgui_acceptance>"
    COMMENT "Copy matching OpenCASCADE runtime DLLs"
  )

  add_custom_command(TARGET cxvision_imgui_acceptance POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OpenCV_DIR}/x64/vc16/bin/opencv_world490.dll"
            "$<TARGET_FILE_DIR:cxvision_imgui_acceptance>"
    COMMENT "Copy OpenCV runtime DLL"
  )

  if(TARGET libtorch_module_runtime)
    add_custom_command(TARGET cxvision_imgui_acceptance POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "$<TARGET_FILE:libtorch_module_runtime>"
              "$<TARGET_FILE_DIR:cxvision_imgui_acceptance>"
      COMMENT "Copy libtorch_module_runtime.dll for torch module acceptance"
    )

    file(GLOB CXVISION_LIBTORCH_RUNTIME_DLLS
         "${LIBTORCH_ROOT}/lib/*.dll")
    foreach(_cxvision_libtorch_runtime_dll IN LISTS CXVISION_LIBTORCH_RUNTIME_DLLS)
      add_custom_command(TARGET cxvision_imgui_acceptance POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_cxvision_libtorch_runtime_dll}"
                "$<TARGET_FILE_DIR:cxvision_imgui_acceptance>"
        COMMENT "Copy LibTorch runtime DLL ${_cxvision_libtorch_runtime_dll}"
      )
    endforeach()
  else()
    message(STATUS
      "libtorch_module_runtime target is unavailable; runtime DLL copy is skipped")
  endif()
else()
  message(STATUS "Runtime DLL post-build copy is disabled (CXVISION_COPY_RUNTIME_DLLS=OFF)")
endif()



option(CXVISION_BUILD_CXPARSER_RETURN_TESTS
       "Build cxparser return keyword regression"
       ON)

if(CXVISION_BUILD_CXPARSER_RETURN_TESTS)
  add_subdirectory(tests/cxparser_return)
endif()
