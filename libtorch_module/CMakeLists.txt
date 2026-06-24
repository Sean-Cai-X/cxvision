# NOTE: formatting normalized during the libtorch_module cleanup pass.

cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(libtorch_module_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LIBTORCH_MODULE_BUILD_MINIMAL "Build pure LibTorch minimal validation target" ON)
option(LIBTORCH_MODULE_BUILD_FULL "Build full validation target with OpenCV-dependent tests" ON)
option(LIBTORCH_MODULE_BUILD_CONTRACT "Build low-coupling contract smoke target" ON)
option(LIBTORCH_MODULE_BUILD_MOBILEVIT "Build MobileViTv2 train/infer validation target" ON)
option(LIBTORCH_MODULE_BUILD_SEGMENTATION "Build segmentation train/infer validation target" ON)
option(LIBTORCH_MODULE_BUILD_DEMO "Build demo entry for staged validation runs" ON)
option(LIBTORCH_MODULE_FULL_ENABLE_OPENCV_IMAGE "Enable OpenCV image-processing validation in full target" ON)
option(LIBTORCH_MODULE_FULL_ENABLE_OPENCV_ANNOTATION "Enable OpenCV image+annotation validation in full target" ON)
option(LIBTORCH_MODULE_FULL_ENABLE_DATASET_STAGE "Enable dataset-backed validation in full target" ON)
option(LIBTORCH_MODULE_FULL_ENABLE_EXTERNAL_WEIGHTS "Enable external-weight validation in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_TRAIN_STAGE "Enable training-oriented validation in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_INFER_STAGE "Enable image-file inference validation in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_LOAD "Enable MobileViTv2 weight-load smoke in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_TRAIN "Enable MobileViTv2 train-step smoke in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_DATASET "Enable MobileViTv2 dataset smoke in full target" ON)
option(LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_FULLTRAIN "Enable MobileViTv2 dataset-backed full train smoke in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_TWOSTAGE_INFER "Enable two-stage inference smoke in full target" ON)
option(LIBTORCH_MODULE_FULL_ENABLE_TWOSTAGE_TRAIN "Enable two-stage train smoke in full target" OFF)
option(LIBTORCH_MODULE_FULL_ENABLE_OCC_STAGE "Enable OCC stage in full target" OFF)

set(_libtorch_module_nvtoolsext_shim "${CMAKE_CURRENT_SOURCE_DIR}/third_party_shims/NvToolsExt")
if(NOT TARGET CUDA::nvToolsExt AND EXISTS "${_libtorch_module_nvtoolsext_shim}/include")
    add_library(CUDA::nvToolsExt INTERFACE IMPORTED)
    set_target_properties(CUDA::nvToolsExt PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_libtorch_module_nvtoolsext_shim}/include")
endif()

set(OpenCV_DIR "D:/opencv4.9/opencv/build" CACHE PATH "OpenCV build directory")
find_package(OpenCV QUIET)
find_package(CUDAToolkit REQUIRED)

set(LIBTORCH_ROOT "D:/libtorch2.2.2/libtorch" CACHE PATH "LibTorch root")
set(GPUSDK_ROOT "D:/GPUSDK" CACHE PATH "GPU SDK root")
set(TORCH_INSTALL_PREFIX "${LIBTORCH_ROOT}")

set(LIBTORCH_INCLUDE_DIRS
    "${LIBTORCH_ROOT}/include"
    "${LIBTORCH_ROOT}/include/torch/csrc/api/include"
)
set(LIBTORCH_LIBRARY_DIR "${LIBTORCH_ROOT}/lib")
set(GPUSDK_LIBRARY_DIR "${GPUSDK_ROOT}/lib/x64")
set(LIBTORCH_MODULE_LIBRARIES
    c10_cuda.lib
    torch_cuda.lib
    torch.lib
    torch_cpu.lib
    c10.lib
    caffe2_nvrtc.lib
    CUDA::cudart
)

foreach(_full_option IN ITEMS
    LIBTORCH_MODULE_FULL_ENABLE_OPENCV_IMAGE
    LIBTORCH_MODULE_FULL_ENABLE_OPENCV_ANNOTATION
    LIBTORCH_MODULE_FULL_ENABLE_DATASET_STAGE
    LIBTORCH_MODULE_FULL_ENABLE_EXTERNAL_WEIGHTS
    LIBTORCH_MODULE_FULL_ENABLE_TRAIN_STAGE
    LIBTORCH_MODULE_FULL_ENABLE_INFER_STAGE
    LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_LOAD
    LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_TRAIN
    LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_DATASET
    LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_FULLTRAIN
    LIBTORCH_MODULE_FULL_ENABLE_TWOSTAGE_INFER
    LIBTORCH_MODULE_FULL_ENABLE_TWOSTAGE_TRAIN
    LIBTORCH_MODULE_FULL_ENABLE_OCC_STAGE
)
    if(${_full_option})
        set(${_full_option}_VALUE 1)
    else()
        set(${_full_option}_VALUE 0)
    endif()
endforeach()

enable_testing()

function(configure_libtorch_module_target target_name)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LIBTORCH_INCLUDE_DIRS}
        ${GPUSDK_ROOT}/include
        ${CUDAToolkit_INCLUDE_DIRS}
    )

    target_link_directories(${target_name} PRIVATE
        ${LIBTORCH_LIBRARY_DIR}
        ${GPUSDK_LIBRARY_DIR}
    )

    target_link_libraries(${target_name} PRIVATE
        ${LIBTORCH_MODULE_LIBRARIES}
    )

    if(MSVC)
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        target_compile_definitions(${target_name} PRIVATE
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
        )
        target_compile_options(${target_name} PRIVATE /EHsc)
        target_link_options(${target_name} PRIVATE
            "/INCLUDE:?warp_size@cuda@at@@YAHXZ"
            "/INCLUDE:?ignore_this_library_placeholder@@YAHXZ"
        )
    endif()

    set_property(TARGET ${target_name} PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

function(configure_libtorch_module_test_environment test_name)
    set(_test_path "${LIBTORCH_LIBRARY_DIR};${OpenCV_DIR}/x64/vc16/bin;${CUDAToolkit_BIN_DIR};$ENV{PATH}")
    string(REPLACE ";" "\\;" _test_path "${_test_path}")
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
            "PATH=${_test_path};NVTOOLSEXT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/third_party_shims/NvToolsExt")
endfunction()

function(configure_libtorch_module_full_definitions target_name)
    target_compile_definitions(${target_name} PRIVATE
        TORCH_FULL_ENABLE_OPENCV_IMAGE=${LIBTORCH_MODULE_FULL_ENABLE_OPENCV_IMAGE_VALUE}
        TORCH_FULL_ENABLE_OPENCV_ANNOTATION=${LIBTORCH_MODULE_FULL_ENABLE_OPENCV_ANNOTATION_VALUE}
        TORCH_FULL_ENABLE_DATASET_STAGE=${LIBTORCH_MODULE_FULL_ENABLE_DATASET_STAGE_VALUE}
        TORCH_FULL_ENABLE_EXTERNAL_WEIGHTS=${LIBTORCH_MODULE_FULL_ENABLE_EXTERNAL_WEIGHTS_VALUE}
        TORCH_FULL_ENABLE_TRAIN_STAGE=${LIBTORCH_MODULE_FULL_ENABLE_TRAIN_STAGE_VALUE}
        TORCH_FULL_ENABLE_INFER_STAGE=${LIBTORCH_MODULE_FULL_ENABLE_INFER_STAGE_VALUE}
        TORCH_FULL_ENABLE_MOBILEVIT_LOAD=${LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_LOAD_VALUE}
        TORCH_FULL_ENABLE_MOBILEVIT_TRAIN=${LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_TRAIN_VALUE}
        TORCH_FULL_ENABLE_MOBILEVIT_DATASET=${LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_DATASET_VALUE}
        TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN=${LIBTORCH_MODULE_FULL_ENABLE_MOBILEVIT_FULLTRAIN_VALUE}
        TORCH_FULL_ENABLE_TWOSTAGE_INFER=${LIBTORCH_MODULE_FULL_ENABLE_TWOSTAGE_INFER_VALUE}
        TORCH_FULL_ENABLE_TWOSTAGE_TRAIN=${LIBTORCH_MODULE_FULL_ENABLE_TWOSTAGE_TRAIN_VALUE}
        TORCH_FULL_ENABLE_OCC_STAGE=${LIBTORCH_MODULE_FULL_ENABLE_OCC_STAGE_VALUE}
    )
endfunction()

if(LIBTORCH_MODULE_BUILD_MINIMAL)
    add_executable(libtorch_module_minimal_tests
        torch_minimal_smoke.cpp
    )
    configure_libtorch_module_target(libtorch_module_minimal_tests)
    add_test(NAME libtorch_module_minimal_tests COMMAND libtorch_module_minimal_tests)
    configure_libtorch_module_test_environment(libtorch_module_minimal_tests)
endif()

if(LIBTORCH_MODULE_BUILD_CONTRACT)
    add_executable(libtorch_module_contract_tests
        torch_contract_smoke.cpp
    )
    configure_libtorch_module_target(libtorch_module_contract_tests)
    add_test(NAME libtorch_module_contract_tests COMMAND libtorch_module_contract_tests)
    configure_libtorch_module_test_environment(libtorch_module_contract_tests)
    set_property(TEST libtorch_module_contract_tests APPEND PROPERTY
        ENVIRONMENT "LIBTORCH_MODULE_USE_CUDA=0")
endif()

if(LIBTORCH_MODULE_BUILD_FULL)
    if(OpenCV_FOUND)
        add_executable(libtorch_module_full_tests
            torch_test_main.cpp
        )
        configure_libtorch_module_target(libtorch_module_full_tests)
        target_link_libraries(libtorch_module_full_tests PRIVATE ${OpenCV_LIBS})
        configure_libtorch_module_full_definitions(libtorch_module_full_tests)
        add_test(NAME libtorch_module_full_tests COMMAND libtorch_module_full_tests)
        configure_libtorch_module_test_environment(libtorch_module_full_tests)
    else()
        message(STATUS "OpenCV not found. Skipping libtorch_module_full_tests target.")
    endif()
endif()

if(LIBTORCH_MODULE_BUILD_MOBILEVIT)
    if(OpenCV_FOUND)
        add_executable(libtorch_module_mobilevit_stage_tests
            torch_mobilevit_test_main.cpp
        )
        configure_libtorch_module_target(libtorch_module_mobilevit_stage_tests)
        target_link_libraries(libtorch_module_mobilevit_stage_tests PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(libtorch_module_mobilevit_stage_tests PRIVATE
            TORCH_FULL_ENABLE_OPENCV_IMAGE=0
            TORCH_FULL_ENABLE_OPENCV_ANNOTATION=0
            TORCH_FULL_ENABLE_DATASET_STAGE=1
            TORCH_FULL_ENABLE_EXTERNAL_WEIGHTS=0
            TORCH_FULL_ENABLE_TRAIN_STAGE=1
            TORCH_FULL_ENABLE_INFER_STAGE=1
            TORCH_FULL_ENABLE_MOBILEVIT_LOAD=0
            TORCH_FULL_ENABLE_MOBILEVIT_TRAIN=1
            TORCH_FULL_ENABLE_MOBILEVIT_DATASET=1
            TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN=0
            TORCH_FULL_ENABLE_TWOSTAGE_INFER=1
            TORCH_FULL_ENABLE_TWOSTAGE_TRAIN=1
            TORCH_FULL_ENABLE_OCC_STAGE=0
        )
        add_test(
            NAME libtorch_module_mobilevit_infer_tests
            COMMAND libtorch_module_mobilevit_stage_tests --mode infer)
        configure_libtorch_module_test_environment(libtorch_module_mobilevit_infer_tests)
        add_test(
            NAME libtorch_module_mobilevit_train_tests
            COMMAND libtorch_module_mobilevit_stage_tests --mode train)
        configure_libtorch_module_test_environment(libtorch_module_mobilevit_train_tests)
    else()
        message(STATUS "OpenCV not found. Skipping libtorch_module_mobilevit_stage_tests target.")
    endif()
endif()

if(LIBTORCH_MODULE_BUILD_SEGMENTATION)
    add_executable(libtorch_module_segmentation_stage_tests
        torch_segmentation_test_main.cpp
    )
    configure_libtorch_module_target(libtorch_module_segmentation_stage_tests)
    add_test(
        NAME libtorch_module_segmentation_infer_tests
        COMMAND libtorch_module_segmentation_stage_tests --mode infer)
    configure_libtorch_module_test_environment(libtorch_module_segmentation_infer_tests)
    add_test(
        NAME libtorch_module_segmentation_train_tests
        COMMAND libtorch_module_segmentation_stage_tests --mode train)
    configure_libtorch_module_test_environment(libtorch_module_segmentation_train_tests)
    set_property(TEST libtorch_module_segmentation_infer_tests APPEND PROPERTY
        ENVIRONMENT "LIBTORCH_MODULE_USE_CUDA=0")
    set_property(TEST libtorch_module_segmentation_train_tests APPEND PROPERTY
        ENVIRONMENT "LIBTORCH_MODULE_USE_CUDA=0")
endif()

if(LIBTORCH_MODULE_BUILD_DEMO)
    if(OpenCV_FOUND)
        add_executable(libtorch_module_demo
            torch_demo_main.cpp
        )
        configure_libtorch_module_target(libtorch_module_demo)
        target_link_libraries(libtorch_module_demo PRIVATE ${OpenCV_LIBS})
        configure_libtorch_module_full_definitions(libtorch_module_demo)
    else()
        message(STATUS "OpenCV not found. Skipping libtorch_module_demo target.")
    endif()
endif()