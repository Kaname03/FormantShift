cmake_minimum_required(VERSION 3.20)

project(FormantShift LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(JUCE_TAG 8.0.0) # 必要に応じて更新
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        ${JUCE_TAG}
)
FetchContent_MakeAvailable(juce)

# プラグインのサブディレクトリを追加
add_subdirectory(plugins/VoiceModeler)