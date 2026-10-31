# The game logic as a library with a plain C interface, see GeneralsLogic.h.
add_library(z_logiclib SHARED)

set_target_properties(z_logiclib PROPERTIES OUTPUT_NAME "generalszh_logic${RTS_BUILD_OUTPUT_SUFFIX}")

target_sources(z_logiclib PRIVATE
    GeneralsLogic.cpp
    GeneralsLogic.h
)

target_compile_definitions(z_logiclib PRIVATE
    GENERALSLOGIC_EXPORTS
)

# The same libraries as the game executable z_generals.
target_link_libraries(z_logiclib PRIVATE
    comctl32
    imm32
    vfw32
    winmm
    z_gameenginedevice
    zi_always
)

if(MSVC)
    target_link_options(z_logiclib PRIVATE "/NODEFAULTLIB:libci.lib")
endif()

# A console program that uses the library like another engine would: through GeneralsLogic.h only.
add_executable(z_logichost)

set_target_properties(z_logichost PROPERTIES OUTPUT_NAME "generalszh_logichost${RTS_BUILD_OUTPUT_SUFFIX}")

target_sources(z_logichost PRIVATE
    LogicHost.cpp
)

target_link_libraries(z_logichost PRIVATE
    z_logiclib
)

# A scripted gameplay test through GeneralsLogic.h: build a War Factory, produce a tank and
# destroy an enemy unit.
add_executable(z_skirmishtest)

set_target_properties(z_skirmishtest PROPERTIES OUTPUT_NAME "generalszh_skirmishtest${RTS_BUILD_OUTPUT_SUFFIX}")

target_sources(z_skirmishtest PRIVATE
    SkirmishTest.cpp
)

target_link_libraries(z_skirmishtest PRIVATE
    z_logiclib
)
