set(HWPE_SRCS
"src/hwpe.cpp"
    )
vp_model(NAME pulp.simple_hwpe.hwpe
    SOURCES ${HWPE_SRCS}
    )
set(HWPE_INCS
"inc"
)

vp_model_include_directories(
    NAME pulp.simple_hwpe.hwpe
    DIRECTORY ${HWPE_INCS}
    )

vp_model_compile_options(NAME pulp.simple_hwpe.hwpe OPTIONS)