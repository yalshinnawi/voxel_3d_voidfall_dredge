cmake_minimum_required(VERSION 3.20)
project(VoidfallDredge CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure runtime library and multi-processor compilation for MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # /MP: Multi-processor parallel compilation across all CPU cores
    add_compile_options(/W3 /utf-8 /permissive- /MP)
    # Aggressive optimization, intrinsic expansion, and dead-code elimination for Release
    add_compile_options("$<$<CONFIG:Release>:/O2;/Oi;/Ot;/Gy>")
    add_link_options("$<$<CONFIG:Release>:/OPT:REF;/OPT:ICF>")
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

# ── Platform OpenGL ──
if(APPLE)
    set(OPENGL_LIBS "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    set(OPENGL_LIBS OpenGL::GL)
endif()

# ── Core Engine Static Library (Compiled ONCE for main app & all test targets) ──
set(ENGINE_CORE_SOURCES
    src/core/logger.cpp
    src/core/glad/glad.c
    src/core/window.cpp
    src/core/save_system.cpp
    src/player/character_class.cpp
    src/player/upgrades.cpp
    src/player/controller.cpp
    src/voxel/chunk.cpp
    src/voxel/greedy_mesher.cpp
    src/voxel/level_shapes.cpp
    src/voxel/world.cpp
    src/voxel/structural_check.cpp
    src/voxel/fluid_sim.cpp
    src/skills/surveying.cpp
    src/systems/hazard_clock.cpp
    src/systems/extraction.cpp
    src/entities/dynamic_debris.cpp
    src/entities/enemies/void_stalker.cpp
    src/entities/enemies/seismic_burrower.cpp
    src/audio/audio_engine.cpp
    src/systems/audio_system.cpp
    src/ai/aberrant_ai.cpp
    src/ai/spawn_manager.cpp
    src/ai/swarm_manager.cpp
    src/systems/stealth_system.cpp
    src/entities/carcass_manager.cpp
    src/entities/flare.cpp
    src/systems/mission_system.cpp
    src/graphics/animation_controller.cpp
    src/graphics/shader.cpp
    src/graphics/clustered_lighting.cpp
    src/graphics/particle_system.cpp
    src/ui/hud.cpp
    src/ui/debrief_menu.cpp
    src/entities/vault_door.cpp
    src/ui/terrain_scanner.cpp
    src/graphics/surface_clutter.cpp
)

add_library(voidfall_engine_core STATIC ${ENGINE_CORE_SOURCES})

target_compile_definitions(voidfall_engine_core PUBLIC
    GLFW_INCLUDE_NONE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_include_directories(voidfall_engine_core PUBLIC
    include
    src
)

target_link_libraries(voidfall_engine_core PUBLIC
    glfw
    glm::glm
    ${OPENGL_LIBS}
)

if(WIN32)
    target_link_libraries(voidfall_engine_core PUBLIC ws2_32 winmm)
endif()

# Precompiled headers: drastically accelerates MSVC build times during rapid TDD
target_precompile_headers(voidfall_engine_core PRIVATE src/core/pch.hpp)

# ── Main Game Executable ──
set(APP_SOURCES
    src/main.cpp
    src/core/application.cpp
    src/core/screenshot.cpp
    src/graphics/texture_array.cpp
    src/graphics/renderer.cpp
    src/graphics/viewmodel.cpp
    src/ui/orbital_hub.cpp
    src/ui/pause_menu.cpp
    src/net/net_host.cpp
    src/net/net_client.cpp
)

add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_precompile_headers(${PROJECT_NAME} REUSE_FROM voidfall_engine_core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    voidfall_engine_core
)

if(BUILD_WITH_GNS)
    target_link_libraries(${PROJECT_NAME} PRIVATE GameNetworkingSockets::GameNetworkingSockets)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GAMENETWORKINGSOCKETS=1)
endif()

# ── Copy assets directory to build output directory ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# ── Test Executables (Instant link against voidfall_engine_core) ──
add_executable(test_progression tests/test_progression.cpp)
target_link_libraries(test_progression PRIVATE voidfall_engine_core)

add_executable(test_unit_all tests/test_unit_all.cpp)
target_link_libraries(test_unit_all PRIVATE voidfall_engine_core)

add_executable(test_e2e_expeditions tests/test_e2e_expeditions.cpp)
target_link_libraries(test_e2e_expeditions PRIVATE voidfall_engine_core)

add_executable(test_level_collision tests/test_level_collision.cpp)
target_link_libraries(test_level_collision PRIVATE voidfall_engine_core)

add_executable(test_enemy_stalker tests/test_enemy_stalker.cpp)
target_link_libraries(test_enemy_stalker PRIVATE voidfall_engine_core)

add_executable(test_enemy_burrower tests/test_enemy_burrower.cpp)
target_link_libraries(test_enemy_burrower PRIVATE voidfall_engine_core)

add_executable(test_audio tests/test_audio.cpp)
target_link_libraries(test_audio PRIVATE voidfall_engine_core)

add_executable(test_audio_system tests/test_audio_system.cpp)
target_link_libraries(test_audio_system PRIVATE voidfall_engine_core)

add_executable(test_combat_omni_ai tests/test_combat_omni_ai.cpp)
target_link_libraries(test_combat_omni_ai PRIVATE voidfall_engine_core)

add_executable(test_gameplay_mechanics tests/test_gameplay_mechanics.cpp)
target_link_libraries(test_gameplay_mechanics PRIVATE voidfall_engine_core)

add_executable(test_spawn_safety tests/test_spawn_safety.cpp)
target_link_libraries(test_spawn_safety PRIVATE voidfall_engine_core)

add_executable(test_load_and_stress tests/test_load_and_stress.cpp)
target_link_libraries(test_load_and_stress PRIVATE voidfall_engine_core)

add_executable(test_voxel_geometry tests/test_voxel_geometry.cpp)
target_link_libraries(test_voxel_geometry PRIVATE voidfall_engine_core)

add_executable(test_fluid_simulation tests/test_fluid_simulation.cpp)
target_link_libraries(test_fluid_simulation PRIVATE voidfall_engine_core)

add_executable(test_fluid_permeation tests/test_fluid_permeation.cpp)
target_link_libraries(test_fluid_permeation PRIVATE voidfall_engine_core)

add_executable(test_fluid_physics tests/test_fluid_physics.cpp)
target_link_libraries(test_fluid_physics PRIVATE voidfall_engine_core)

# ── Reuse Precompiled Headers Across All Test Executables ──
set(VOIDFALL_TEST_TARGETS
    test_progression
    test_unit_all
    test_e2e_expeditions
    test_level_collision
    test_enemy_stalker
    test_enemy_burrower
    test_audio
    test_audio_system
    test_combat_omni_ai
    test_gameplay_mechanics
    test_spawn_safety
    test_load_and_stress
    test_voxel_geometry
    test_fluid_simulation
    test_fluid_permeation
    test_fluid_physics
)
foreach(tgt ${VOIDFALL_TEST_TARGETS})
    target_precompile_headers(${tgt} REUSE_FROM voidfall_engine_core)
endforeach()

# ── CTest Integration (Enables `ctest --parallel` and test discovery) ──
enable_testing()
add_test(NAME UnitTests COMMAND test_unit_all)
add_test(NAME ProgressionTests COMMAND test_progression)
add_test(NAME E2ETests COMMAND test_e2e_expeditions)
add_test(NAME LevelCollisionTests COMMAND test_level_collision)
add_test(NAME EnemyStalkerTests COMMAND test_enemy_stalker)
add_test(NAME EnemyBurrowerTests COMMAND test_enemy_burrower)
add_test(NAME AudioTests COMMAND test_audio)
add_test(NAME AudioSystemTests COMMAND test_audio_system)
add_test(NAME CombatOmniAITests COMMAND test_combat_omni_ai)
add_test(NAME GameplayMechanicsTests COMMAND test_gameplay_mechanics)
add_test(NAME SpawnSafetyTests COMMAND test_spawn_safety)
add_test(NAME LoadAndStressTests COMMAND test_load_and_stress)
add_test(NAME VoxelGeometryTests COMMAND test_voxel_geometry)
add_test(NAME FluidSimulationTests COMMAND test_fluid_simulation)
add_test(NAME FluidPermeationTests COMMAND test_fluid_permeation)
add_test(NAME FluidPhysicsTests COMMAND test_fluid_physics)


