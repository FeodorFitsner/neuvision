project(ZCoreTests LANGUAGES CXX)

find_package(Qt5 COMPONENTS Test REQUIRED)

add_executable(${PROJECT_NAME}
  tst_zsettingsitem.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Test
    ZCore)

add_test(NAME ${PROJECT_NAME} COMMAND $<TARGET_FILE:${PROJECT_NAME}>)
