cmake_minimum_required(VERSION 3.24)

project(USC-Auto VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认 Release（性能更好的同时避免 assert 干扰）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Windows: WIN32 子系统（GUI 程序，无控制台黑窗口）
if(WIN32)
  add_compile_options(-mwindows)
endif()

# Qt: 禁用 moc 对 MOC 特性的依赖猜测
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
find_package(cpr REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)

add_executable(usc-auto WIN32
  src/main.cpp
  src/Logger.cpp
  src/AppConfig.cpp
  src/WlanChecker.cpp
  src/NetworkMonitor.cpp
  src/AuthClient.cpp
  src/CoreWorker.cpp
  src/LogWindow.cpp
  src/TrayController.cpp
)

target_link_libraries(usc-auto PRIVATE
  Qt6::Widgets
  cpr::cpr
  nlohmann_json::nlohmann_json
  wlanapi
  iphlpapi
  ws2_32
)

# MSVC 风格的 /W4 等价物（MinGW 下用 -Wall -Wextra -Wpedantic）
target_compile_options(usc-auto PRIVATE -Wall -Wextra -Wpedantic)

# UTF-8 源码与执行字符集（保证中文日志/界面不乱码）
if(WIN32)
  target_compile_options(usc-auto PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

install(TARGETS usc-auto RUNTIME DESTINATION .)
