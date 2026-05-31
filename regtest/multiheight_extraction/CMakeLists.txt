# Multi-height extraction test
add_test(NAME regtest_multiheight_extraction
         COMMAND wind_solver inputs.i
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction)
set_tests_properties(regtest_multiheight_extraction PROPERTIES LABELS "regtest")

# Check that multiple extraction files were created
add_test(NAME regtest_multiheight_extraction_check_10m
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction/wind_extract_10m.csv
                 ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction/wind_extract_10m.csv
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction)
set_tests_properties(regtest_multiheight_extraction_check_10m PROPERTIES 
                     LABELS "regtest" 
                     DEPENDS regtest_multiheight_extraction)

add_test(NAME regtest_multiheight_extraction_check_50m
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction/wind_extract_50m.csv
                 ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction/wind_extract_50m.csv
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction)
set_tests_properties(regtest_multiheight_extraction_check_50m PROPERTIES 
                     LABELS "regtest" 
                     DEPENDS regtest_multiheight_extraction)

add_test(NAME regtest_multiheight_extraction_check_100m
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction/wind_extract_100m.csv
                 ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction/wind_extract_100m.csv
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/multiheight_extraction)
set_tests_properties(regtest_multiheight_extraction_check_100m PROPERTIES 
                     LABELS "regtest" 
                     DEPENDS regtest_multiheight_extraction)
