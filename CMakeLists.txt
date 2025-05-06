cmake_minimum_required(VERSION 3.28)
project(s2c CXX)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(X11 COMPONENTS Xtst)

add_executable(s2c Space2Ctrl.cpp)
target_link_libraries(s2c PUBLIC X11::Xtst)

install(CODE [[
  cmake_minimum_required(VERSION 3.28)
  file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
  file(REAL_PATH "${CMAKE_INSTALL_PREFIX}" ABS_PREFIX EXPAND_TILDE)
  file(READ "${CMAKE_SOURCE_DIR}/s2c.service.in" service_content)
  string(REPLACE "@INSTALL_PREFIX@" "${ABS_PREFIX}" service_content "${service_content}")
  set(output_path "$ENV{HOME}/.config/systemd/user/s2c.service")
  message(STATUS "Installing service file ${output_path}")
  file(WRITE "${output_path}" "${service_content}")
#  file(CHMOD "${output_path}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
]])

install(TARGETS s2c RUNTIME DESTINATION bin)
install(PROGRAMS ${PROJECT_SOURCE_DIR}/s2cctl DESTINATION bin)
