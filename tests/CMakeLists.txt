add_executable(test_image_io test_image_io.cpp)
target_link_libraries(test_image_io PRIVATE fsic_core)
add_test(NAME test_image_io COMMAND test_image_io)

add_executable(test_rowsig test_rowsig.cpp)
target_link_libraries(test_rowsig PRIVATE fsic_core)
add_test(NAME test_rowsig COMMAND test_rowsig)

add_executable(test_stitch test_stitch.cpp)
target_link_libraries(test_stitch PRIVATE fsic_core)
add_test(NAME test_stitch COMMAND test_stitch)

add_executable(test_probe test_probe.cpp)
target_link_libraries(test_probe PRIVATE fsic_platform)
add_test(NAME test_probe COMMAND test_probe)

add_executable(test_session_synthetic test_session_synthetic.cpp)
target_link_libraries(test_session_synthetic PRIVATE fsic_platform)
add_test(NAME test_session_synthetic COMMAND test_session_synthetic)
