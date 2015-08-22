find_package(PkgConfig REQUIRED)
pkg_check_modules(EventdEvent REQUIRED libeventd-event>=0.3)
pkg_check_modules(EventdPlugin REQUIRED libeventd-plugin>=0.3)

find_package(PythonLibs 3.3 REQUIRED)

include_directories(SYSTEM
    ${EventdEvent_INCLUDE_DIRS}
    ${EventdPlugin_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIR})

add_library(fedmsg MODULE
    fedmsg.c)
target_link_libraries(fedmsg
    PRIVATE
        ${EventdEvent_LIBRARIES}
        ${EventdPlugin_LIBRARIES}
        ${PYTHON_LIBRARY})

install(
    TARGETS     fedmsg
    DESTINATION lib${LIB_SUFFIX}/eventd
    COMPONENT   runtime)
