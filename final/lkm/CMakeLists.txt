CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT("CS370 Final LKM")

SET(SOURCES
    Makefile
    parcel.h
    parcel.c
    treefs.h
    treefs.c
    #super.c
    #inode.c
)

INCLUDE_DIRECTORIES(
    /usr/lib/modules/4.10.11-1-ARCH/build/include
)

ADD_CUSTOM_TARGET(lkm-dummy SOURCES ${SOURCES})

ADD_CUSTOM_TARGET(lkm ALL
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND make
)

