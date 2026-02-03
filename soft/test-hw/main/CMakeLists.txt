file(GLOB test_srcs "*.cpp")

set(srcs
    ${test_srcs}
)

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    EMBED_FILES test1_test_imp.bin
                    )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
