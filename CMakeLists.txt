cmake_minimum_required(VERSION 3.16)
project(mydb VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8 /EHsc)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# mydb_core — the database engine (storage, indexes, query processing).
# Plain C++17 with no OS-specific code.
# ---------------------------------------------------------------------------
add_library(mydb_core STATIC
    src/catalog/Type.cpp
    src/catalog/Catalog.cpp
    src/storage/DiskManager.cpp
    src/storage/Page.cpp
    src/storage/BufferPool.cpp
    src/storage/HeapFile.cpp
    src/storage/Tuple.cpp
    src/index/BTree.cpp
    src/engine/Database.cpp
    src/query/AST.cpp
    src/query/Lexer.cpp
    src/query/Parser.cpp
    src/query/Expression.cpp
    src/query/Optimizer.cpp
    src/query/Planner.cpp
    src/query/Executor.cpp
    src/query/QueryEngine.cpp
    src/operators/SeqScan.cpp
    src/operators/IndexScan.cpp
    src/operators/Filter.cpp
    src/operators/Projection.cpp
    src/operators/NestedLoopJoin.cpp
    src/operators/HashJoin.cpp
    src/operators/Sort.cpp
    src/operators/Aggregate.cpp
    src/operators/Limit.cpp
    src/operators/TableWriter.cpp
    src/operators/Insert.cpp
    src/operators/Update.cpp
    src/operators/Delete.cpp
    # HTTP/JSON/API logic has no socket code, so it lives in the core too
    src/server/Json.cpp
    src/server/Http.cpp
    src/server/Api.cpp
)
target_include_directories(mydb_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Unit tests (run with: ctest --test-dir build)
# ---------------------------------------------------------------------------
enable_testing()
foreach(t storage btree query api)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE mydb_core)
    add_test(NAME ${t} COMMAND test_${t})
endforeach()

# ---------------------------------------------------------------------------
# Server and terminal client — Windows (Winsock).
# ---------------------------------------------------------------------------
if(WIN32)
    add_library(mydb_net STATIC
        src/server/Net.cpp
        src/server/Protocol.cpp
        src/server/Server.cpp
        src/server/Connection.cpp
    )
    target_link_libraries(mydb_net PUBLIC mydb_core ws2_32)
    target_compile_definitions(mydb_net PUBLIC _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)

    add_executable(mydb src/main.cpp)
    target_link_libraries(mydb PRIVATE mydb_net)

    add_executable(mydb_client tools/client.cpp)
    target_link_libraries(mydb_client PRIVATE mydb_net)

    if(MINGW)
        # Ship a self-contained .exe (no libstdc++/libgcc DLLs needed).
        target_link_options(mydb PRIVATE -static)
        target_link_options(mydb_client PRIVATE -static)
    endif()
else()
    message(STATUS "mydb server/client target Windows (Winsock); building engine + tests only.")
endif()
