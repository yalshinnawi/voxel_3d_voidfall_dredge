cmake_minimum_required(VERSION 3.20)
project(VoidfallDredge CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure runtime library for MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/W3 /utf-8 /permissive-)
endif()

include(FetchContent)

# ── GLFW (Windowing & Input) ──
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ── GLM (Mathematics) ──
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# ── GameNetworkingSockets (Valve transport library) ──
option(BUILD_WITH_GNS "Build with GameNetworkingSockets" OFF)

if(BUILD_WITH_GNS)
    FetchContent_Declare(
        GameNetworkingSockets
        GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
        GIT_TAG        v1.4.1
    )
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(USE_STEAMWEBRTC OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(GameNetworkingSockets)
endif()

# ── Project Sources ──
set(SOURCES
    src/main.cpp
    src/core/glad/glad.c
    src/core/window.cpp
    src/core/application.cpp
    src/graphics/shader.cpp
    src/graphics/texture_array.cpp
    src/graphics/renderer.cpp
    src/voxel/chunk.cpp
    src/voxel/greedy_mesher.cpp
    src/voxel/world.cpp
    src/voxel/structural_check.cpp
    src/player/controller.cpp
    src/entities/dynamic_debris.cpp
    src/systems/hazard_clock.cpp
    src/systems/extraction.cpp
    src/ui/hud.cpp
    src/net/net_host.cpp
    src/net/net_client.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    GLFW_INCLUDE_NONE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glm::glm
)

# ── Networking Sockets (Winsock on Windows) ──
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# ── Platform OpenGL ──
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

if(BUILD_WITH_GNS)
    target_link_libraries(${PROJECT_NAME} PRIVATE GameNetworkingSockets::GameNetworkingSockets)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GAMENETWORKINGSOCKETS=1)
endif()

# ── Copy assets directory to build output directory ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
