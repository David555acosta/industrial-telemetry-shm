cmake_minimum_required(VERSION 3.12)
project(rt_telemetry_ipc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.12)
project(rt_telemetry_ipc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pthread)

include_directories(${CMAKE_SOURCE_DIR}/include)

# (Descomentarás esto cuando creemos los archivos en src/)
# add_executable(sensor src/sensor.c)
# target_link_libraries(sensor PRIVATE pthread rt)

# add_executable(monitor src/monitor.c)
# target_link_libraries(monitor PRIVATE pthread rt)