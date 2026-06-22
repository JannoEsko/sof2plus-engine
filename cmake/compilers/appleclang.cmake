# Apple Clang compiler specific settings

if(NOT CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    return()
endif()

add_compile_options(-Wno-int-conversion)

add_compile_options(-Wall -Wimplicit -Wshadow
        -Wstrict-prototypes -Wformat=2  -Wformat-security
        -Wstrict-aliasing=2 -Wmissing-format-attribute
        -Wdisabled-optimization -Werror-implicit-function-declaration)

add_compile_options(-Wno-format-zero-length -Wno-format-nonliteral)

# There are lots of instances of union based aliasing in the code
# that rely on the compiler not optimising them away, so disable it
add_compile_options(-fno-strict-aliasing)

# This is necessary to hide all symbols unless explicitly exported
# via the Q_EXPORT macro
add_compile_options(-fvisibility=hidden)
