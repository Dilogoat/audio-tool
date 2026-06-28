cmake_minimum_required(VERSION 3.24)
project(audio-tool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
endif()

# Core library
add_library(audio_core STATIC
    src/core/AudioFile.cpp
    src/core/Formats.cpp
    src/core/RmsAnalyzer.cpp
    src/core/Framing.cpp
    src/core/Stft.cpp
    src/core/Markers.cpp
    src/core/CompositeScore.cpp
    src/core/SpectralCleaner.cpp
    src/core/Editor.cpp
    src/core/Logger.cpp
    src/core/Pipeline.cpp
    src/core/detectors/PauseDetector.cpp
    src/core/detectors/TransientDetector.cpp
    src/core/detectors/BreathDetector.cpp
    src/core/ml/OnnxInference.cpp
)

target_include_directories(audio_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
)

find_package(SndFile CONFIG REQUIRED)
find_package(FFTW3 CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(audio_core PUBLIC
    SndFile::sndfile
    FFTW3::fftw3
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
)

# Desktop application
add_executable(audio_tool
    src/app/main.cpp
    src/app/App.cpp
)

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(audio_tool PRIVATE
    audio_core
    glfw
    imgui::imgui
    implot::implot
    portaudio
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(audio_tool PRIVATE opengl32)
endif()

# Headless CLI (no GUI dependencies)
add_executable(audio_tool_cli
    src/cli/main.cpp
)

target_link_libraries(audio_tool_cli PRIVATE
    audio_core
    nlohmann_json::nlohmann_json
)

# Unit tests
enable_testing()
find_package(Catch2 CONFIG REQUIRED)

add_executable(audio_tests
    tests/unit/test_rms.cpp
    tests/unit/test_pause.cpp
    tests/unit/test_transient.cpp
    tests/unit/test_stft.cpp
    tests/unit/test_breath.cpp
    tests/unit/test_markers.cpp
    tests/unit/test_composite.cpp
    tests/unit/test_audio_file.cpp
    tests/unit/test_formats.cpp
    tests/unit/test_pipeline.cpp
    tests/unit/test_editor.cpp
)

target_compile_definitions(audio_tests PRIVATE TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

target_include_directories(audio_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
)

target_link_libraries(audio_tests PRIVATE
    audio_core
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(audio_tests)
