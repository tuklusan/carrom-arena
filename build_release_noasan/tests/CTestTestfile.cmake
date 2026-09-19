# CMake generated Testfile for 
# Source directory: /home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests
# Build directory: /home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[rules_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_rules")
set_tests_properties([=[rules_test]=] PROPERTIES  LABELS "unit" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;63;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
add_test([=[physics_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_physics")
set_tests_properties([=[physics_test]=] PROPERTIES  LABELS "unit" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;79;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
add_test([=[ai_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_ai")
set_tests_properties([=[ai_test]=] PROPERTIES  LABELS "unit" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;95;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
add_test([=[trace_circular_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_trace_circular")
set_tests_properties([=[trace_circular_test]=] PROPERTIES  LABELS "unit" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;116;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
add_test([=[integration_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_integration")
set_tests_properties([=[integration_test]=] PROPERTIES  LABELS "integration" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;137;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
add_test([=[regression_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_regression")
set_tests_properties([=[regression_test]=] PROPERTIES  LABELS "regression" TIMEOUT "1200" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;160;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
add_test([=[capture_test]=] "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/tests/test_capture")
set_tests_properties([=[capture_test]=] PROPERTIES  LABELS "regression" TIMEOUT "120" WORKING_DIRECTORY "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan" _BACKTRACE_TRIPLES "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;194;add_test;/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/tests/CMakeLists.txt;0;")
