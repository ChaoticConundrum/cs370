CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT("CS370 Assignment2")

ADD_SUBDIRECTORY(libchaos)

SET(SOURCES
    main.cpp
)

ADD_EXECUTABLE(assignment2 ${SOURCES})
LibChaos_Configure_Target(assignment2)
