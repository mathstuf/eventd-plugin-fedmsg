find_package(PkgConfig REQUIRED)
pkg_check_modules(EventdPlugin REQUIRED libeventd-plugin>=0.3)

include_directories(SYSTEM
    ${EventdPlugin_INCLUDE_DIRS})

add_library(fedmsg MODULE
    fedmsg.c)

install(
    TARGETS     fedmsg
    DESTINATION lib${LIB_SUFFIX}/eventd
    COMPONENT   runtime)
