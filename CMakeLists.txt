cmake_minimum_required(VERSION 3.15)

project(CppForth VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Для редакторов
set(CMAKE_CXX_STANDARD 20)

enable_testing()

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.15.1
)
FetchContent_MakeAvailable(catch2)

# Автоматический поиск исходников
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE TESTS "tests/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_executable(tests ${TESTS} ${HEADERS})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
include(Catch)
catch_discover_tests(tests
  DISCOVERY_MODE PRE_TEST       # безопасно для кросс-компиляции и CI
  ADD_TAGS_AS_LABELS            # теги [unit], [slow] и т.п. станут лейблами CTest
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Подключение заголовков (важно для IDE)
target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(
        ${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
endif()
