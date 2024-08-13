set(HWPE_SRCS
"src/input_load.cpp"
"src/input_layout.cpp"
"src/weight_load.cpp"
"src/weight_layout.cpp"
"src/hwpe_weightoffset.cpp"
"src/hwpe_compute.cpp"
"src/output_store.cpp"
"src/hwpe_fsm.cpp"
"src/hwpe.cpp"
    )
vp_model(NAME pulp.simple_hwpe.hwpe
    SOURCES ${HWPE_SRCS}
    )
set(HWPE_INCS
"../neureka/inc/binconv"
"../neureka/inc/buffer"
"inc"
)

vp_model_include_directories(
    NAME pulp.simple_hwpe.hwpe
    DIRECTORY ${HWPE_INCS}
    )

vp_model_compile_options(NAME pulp.simple_hwpe.hwpe OPTIONS)