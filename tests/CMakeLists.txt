add_executable(td1_properties test_properties.cpp)
target_link_libraries(td1_properties PRIVATE td1_core)
add_test(NAME td1_properties COMMAND td1_properties)

add_executable(td1_golden test_golden.cpp)
target_link_libraries(td1_golden PRIVATE td1_core)

find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
  set(TD1_GOLDEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/golden)
  add_test(NAME td1_golden_render
    COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/golden/render_reference.mjs
            ${PROJECT_SOURCE_DIR}/prototype/foresight_td1_v3.html ${TD1_GOLDEN_DIR})
  set_tests_properties(td1_golden_render PROPERTIES FIXTURES_SETUP golden)
  add_test(NAME td1_golden COMMAND td1_golden ${TD1_GOLDEN_DIR})
  set_tests_properties(td1_golden PROPERTIES FIXTURES_REQUIRED golden)
else()
  message(WARNING "node not found: golden-file tests against the JS reference are disabled")
endif()
