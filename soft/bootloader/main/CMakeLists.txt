file(GLOB bloader_srcs "*.cpp")
file(GLOB tftp_srcs  "tftp/src/*.cpp")

set(srcs
    ${bloader_srcs}
    ${tftp_srcs}
)

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "." "./tftp/include"
                    )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
