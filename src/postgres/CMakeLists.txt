add_library(PostgresCheck MODULE "")
target_compile_options(PostgresCheck PRIVATE -fno-rtti)
target_include_directories(PostgresCheck
    PRIVATE
        ${CLANG_INCLUDE_DIRS}
        ${LLVM_INCLUDE_DIRS}
)
target_sources(PostgresCheck PRIVATE ${CMAKE_CURRENT_LIST_DIR}/PostgresCheck.cpp)
