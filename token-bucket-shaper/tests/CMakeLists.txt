# Tests link against tbcore (see top-level CMakeLists.txt).
foreach(t token_bucket bounded_queue statistics config policer_shaper)
    add_executable(test_${t} test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE tbcore)
    add_test(NAME ${t} COMMAND test_${t})
endforeach()
