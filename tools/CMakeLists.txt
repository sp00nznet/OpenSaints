# OpenSaints tools

add_executable(vpp_extract
    vpp_extract.cpp
)

target_link_libraries(vpp_extract PRIVATE opensaints_core)
