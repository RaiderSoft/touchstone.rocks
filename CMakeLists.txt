cmake_minimum_required(VERSION 3.14)
project(touchstone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Download GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Download board library (provides board_lib target + raylib)
FetchContent_Declare(
    boardgame
    GIT_REPOSITORY https://github.com/cs257w26/boardgame.git
    GIT_TAG dev
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(boardgame)

# Download nlohmann/json for Anthropic API client
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# libcurl (system-installed on macOS)
find_package(CURL REQUIRED)

# Go rules engine (static library, no UI dependencies)
add_library(go_engine
    src/board.cpp
    src/move.cpp
    src/game.cpp
    src/scoring.cpp
)
target_include_directories(go_engine PUBLIC include)

# Touchstone — tangible Go with KataGo AI, voice, and vision
add_executable(touchstone
    src/main.cpp
    src/ui/game_board.cpp
    src/ui/game_mode.cpp
    src/chat/chat_overlay.cpp
    src/chat/ai_provider.cpp
    src/engine/katago_engine.cpp
    src/persist/game_save.cpp
    src/persist/dotenv.cpp
    src/audio/audio_encoding.cpp
    src/audio/audio_io.cpp
    cards/cards.cpp
    cards/card_data.cpp
)
target_include_directories(touchstone PRIVATE include cards src)
target_link_libraries(touchstone PRIVATE go_engine board_lib CURL::libcurl nlohmann_json::nlohmann_json)
if(APPLE)
    target_link_libraries(touchstone PRIVATE "-framework AudioToolbox")
elseif(UNIX)
    find_package(ALSA REQUIRED)
    target_link_libraries(touchstone PRIVATE ALSA::ALSA)
endif()

# OpenCV vision component for camera-based board detection
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio)
add_library(touchstone_vision
    src/vision/vision.cpp
    src/vision/calibration.cpp
)
target_include_directories(touchstone_vision PUBLIC src)
target_link_libraries(touchstone_vision PUBLIC ${OpenCV_LIBS})
target_link_libraries(touchstone PRIVATE touchstone_vision)
target_sources(touchstone PRIVATE src/vision/vision_dev.cpp)

# Tests
enable_testing()
add_executable(go_tests
    tests/test_board.cpp
    tests/test_neighbors.cpp
    tests/test_groups.cpp
    tests/test_liberties.cpp
    tests/test_capture.cpp
    tests/test_suicide.cpp
    tests/test_ko.cpp
    tests/test_scoring.cpp
    tests/test_complex.cpp
    tests/test_game_flow.cpp
)
target_include_directories(go_tests PRIVATE include)
target_link_libraries(go_tests PRIVATE go_engine GTest::gtest_main)
add_test(NAME GoTests COMMAND go_tests)

add_executable(ai_tests
    tests/test_ai.cpp
    src/chat/ai_provider.cpp
    src/audio/audio_encoding.cpp
    src/persist/dotenv.cpp
)
target_include_directories(ai_tests PRIVATE include src)
target_link_libraries(ai_tests PRIVATE CURL::libcurl nlohmann_json::nlohmann_json GTest::gtest_main)
add_test(NAME AITests COMMAND ai_tests)

add_executable(katago_tests
    tests/test_katago_coords.cpp
    src/engine/katago_engine.cpp
    src/engine/gtp_client.cpp
)
target_include_directories(katago_tests PRIVATE include src)
target_link_libraries(katago_tests PRIVATE go_engine nlohmann_json::nlohmann_json GTest::gtest_main)
add_test(NAME KataGoTests COMMAND katago_tests)

add_executable(tromp_tests
    tests/test_scoring_comparison.cpp
)
target_include_directories(tromp_tests PRIVATE include)
target_link_libraries(tromp_tests PRIVATE go_engine GTest::gtest_main)
add_test(NAME TrompTests COMMAND tromp_tests)
