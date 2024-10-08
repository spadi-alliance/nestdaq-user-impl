#
cmake_minimum_required(VERSION 3.11)

project(nestdaq LANGUAGES CXX C)

message(STATUS "PROJECT_NAME: ${PROJECT_NAME}")

# check c++ compiler and turn on colored output
message("c++ compiler ID is ${CMAKE_CXX_COMPILER_ID}")
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  message("turnn on colored error message of GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  message("turn on colored error message of Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

#------------------------------------------------------------------------------
# C++ standard level
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MY_CXX_STANDARD 17)
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${MY_CXX_STANDARD})
elseif(${CMAKE_CXX_STANDARD} LESS ${MY_CXX_STANDARD})
  message(FATAL_ERROR "A minimum CMAKE_CXX_STANDARD of ${MY_CXX_STANDARD} is required.")
elseif(${CMAKE_CXX_STANDARD} GREATER ${MY_CXX_STANDARD})
  message(WARNING "A CMAKE_CXX_STANDARD of ${CMAKE_CXX_STANDARD} (greater than ${MY_CXX_STANDARD}) is not tested. Use on your on risk.")
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wfloat-equal -O3")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG -march=native")
message("")
message("CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message("CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message("CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS_MINSIZEREL:     ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message("CMAKE_BUILD_TYPE:               ${CMAKE_BUILD_TYPE}")
message("")

#------------------------------------------------------------------------------
include(GNUInstallDirs)
message("CMAKE_INSTALL_PREFIX:     ${CMAKE_INSTALL_PREFIX}")
message("CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")
message("CMAKE_INSTALL_LIBDIR:     ${CMAKE_INSTALL_LIBDIR}")
message("CMAKE_INSTALL_BINDIR:     ${CMAKE_INSTALL_BINDIR}")
message("CMAKE_INSTALL_DATADIR:    ${CMAKE_INSTALL_DATADIR}")
message("")

find_package(Threads REQUIRED)

#------------------------------------------------------------------------------
# RPATH handling
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX/${CMAKE_INSTALL_LIBDIR}}" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-new-dtags")
    set(CMAKE_INSTALL_RPATH       "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  else()
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
  endif()
endif()

message("CMAKE_EXE_LINKER_FLAGS:    ${CMAKE_EXE_LINKER_FLAGS}")
message("CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")

#------------------------------------------------------------------------------
# set up FairMQ and Boost
find_package(FairMQ)

if(FairMQ_FOUND)
  message(STATUS "FairMQ found")
  message(STATUS "FairMQ_VERSION:              ${FairMQ_VERSION}")
  message(STATUS "FairMQ_GIT_VERSION:          ${FairMQ_GIT_VERSION}")
  message(STATUS "FairMQ_PACKAGE_DEPENDENCIES: ${FairMQ_PACKAGE_DEPENDENCIES}")
  message(STATUS "FairMQ_PACKAGE_COMPONENTS:   ${FairMQ_PACKAGE_COMPONENTS}")
  message(STATUS "FairMQ_PREFIX:               ${FairMQ_PREFIX}")
  message(STATUS "FairMQ_BINDIR:               ${FairMQ_BINDIR}")
  message(STATUS "FairMQ_INCDIR:               ${FairMQ_INCDIR}")
  message(STATUS "FairMQ_LIBDIR:               ${FairMQ_LIBDIR}")
  message(STATUS "FairMQ_DATADIR:              ${FairMQ_DATADIR}")
  message(STATUS "FairMQ_CMAKEMODDIR:          ${FairMQ_CMAKEMODDIR}")
  message(STATUS "FairMQ_BUILD_TYPE:           ${FairMQ_BUILD_TYPE}")
  message(STATUS "FairMQ_CXX_FLAGS:            ${FairMQ_CXX_FLAGS}")

  foreach(dep IN LISTS FairMQ_PACKAGE_DEPENDENCIES)
    if(FairMQ_${dep}_COMPONENTS)
      message(" dep: ${dep}")
      message("  FairMQ_${dep}_version:    ${FairMQ_${dep}_VERSION}")
      message("  FairMQ_${dep}_COMPONENTS: ${FairMQ_${dep}_COMPONENTS}")
      if(${dep} MATCHES "Boost")
        find_package(${dep} ${FairMQ_${dep}_VERSION} COMPONENTS ${FairMQ_${dep}_COMPONENTS}; iostreams; thread;)
      else()
        find_package(${dep} ${FairMQ_${dep}_VERSION} COMPONENTS ${FairMQ_${dep}_COMPONENTS};)
      endif()
    else()
      message(" dep: ${dep}")
      message("  FairMQ_${dep}_VERSION: ${FairMQ_${dep}_VERSION}")
      find_package(${dep} ${FairMQ_${dep}_VERSION})
    endif()
  endforeach()
else()
  message(WARNING "FairMQ not found")
endif()


message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES:    ${Boost_LIBRARIES}")

if(NOT FairLogger_INCDIR)
  set(FairLogger_INCDIR "${FairMQ_INCDIR}/fairlogger/bundled")
endif()
message(STATUS "FairLogger_INCDIR: ${FairLogger_INCDIR}")

#find_package(fmt)
#if(fmt_DIR)
#  message(STATUS "fmt_DIR: ${fmt_DIR}")
#  set(fmt_LIB fmt::fmt)
#  message(STATUS "fmt_LIB: ${fmt_LIB}")
#endif()
if(FairLogger_INCDIR)
  set(fmt_INCDIR "${FairMQ_INCDIR}/fairlogger/bundled")
  message(STATUS "fmt_INCDIR: ${fmt_INCDIR}")
endif()

#==============================================================================
find_package(ROOT REQUIRED COMPONENTS RIO RHTTP Hist)

# ROOT dictionary
if(ROOT_FOUND)
  message(STATUS "ROOT found")
  message(STATUS "ROOT_INCLUDE_DIRS ${ROOT_INCLUDE_DIRS}")
  message(STATUS "ROOT_LIBRARIES ${ROOT_LIBRARIES}")

#  ROOT_GENERATE_DICTIONARY(G__stl_vector_vector
#    LINKDEF LinkDef/stl_vector_vector_LinkDef.h)
endif()

#==============================================================================
# ---------- add hiredis dependency ----------
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)

message(STATUS "HIREDIS_HEADER: ${HIREDIS_HEADER}")
message(STATUS "HIREDIS_LIB:    ${HIREDIS_LIB}")

# ---------- add redis++ dependency ---------
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)
#find_package(redis++)

message(STATUS "REDIS_PLUS_PLUS_HEADER: ${REDIS_PLUS_PLUS_HEADER}")
message(STATUS "REDIS_PLUS_PLUS_LIB:    ${REDIS_PLUS_PLUS_LIB}")

# ---------- add uhbook dependency ---------
find_path(UHBOOK_HEADER uhbook.cxx ../uhbook/)
#find_library(UHBOOK_LIB uhbook)

message(STATUS "UHBOOK_HEADER:    ${UHBOOK_HEADER}")
#message(STATUS "UHBOOK_LIB:       ${UHBOOK_LIB}")

#==============================================================================
# Sampler(for Emulator), Sink, FileSink, STFBuilder, TimeFrameBuilder
#==============================================================================
foreach(lvar IN ITEMS
  Sampler; Sink; FileSink; STFBuilder;
  TimeFrameBuilder; TFBFilePlayer; STFBFilePlayer;
  TimeFrameSlicerByLogicTiming;
  fltcoin; LogicFilter;
  tfdump;
# onlinedisplay;
  FilterTimeFrameSliceABC;
  FilterTimeFrameSliceBySomething;
  InputRegister;
#  AmQStrTdcDqm;
  Scaler)

  set(EXEC ${lvar})
  add_executable(${EXEC} 
    ${EXEC}.cxx;
  )

  target_include_directories(${EXEC} PUBLIC 
    ${Boost_INCLUDE_DIRS};  
    ${FairMQ_INCDIR};
    ${HIREDIS_HEADER};
    ${REDIS_PLUS_PLUS_HEADER};
    ${UHBOOK_HEADER};
    ${CMAKE_SOURCE_DIR};
#    ${FairLogger_INCDIR};
    ${fmt_INCDIR};
  )

  target_link_directories(${EXEC} PUBLIC
    ${Boost_LIBRARY_DIRS}; 
    ${FairMQ_LIBDIR};
#    ${FairLogger_LIBDIR};
  )

  if ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.8.0)
    AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.8.9))
    target_link_libraries(${EXEC} PUBLIC 
      ${Boost_LIBRARIES};  
      ${HIREDIS_LIB};
      ${REDIS_PLUS_PLUS_LIB};
      ${UHBOOK_LIB};
      fairmq
      FairLogger;
      Utility;
      stdc++fs;
      AmQTdcModule;
    )
  elseif ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.4.55)
    AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.4.56))
    target_link_libraries(${EXEC} PUBLIC 
      ${Boost_LIBRARIES};  
      ${HIREDIS_LIB};
      ${REDIS_PLUS_PLUS_LIB};
      ${UHBOOK_LIB};
      FairMQ;
      FairMQStateMachine;
      FairLogger;
      Utility;
      stdc++fs;
      AmQTdcModule;
    )
  else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
  endif()

  install(TARGETS
    ${EXEC};
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endforeach()

#==============================================================================
# AmQStrSampler (for the read-out of AmQ)
#==============================================================================
add_executable(AmQStrTdcSampler
    AmQStrTdcSampler.cxx;
)

target_include_directories(AmQStrTdcSampler PUBLIC 
    ${Boost_INCLUDE_DIRS};  
    ${FairLogger_INCDIR};
    ${FairMQ_INCDIR};
    ${CMAKE_SOURCE_DIR};
)

target_link_directories(AmQStrTdcSampler PUBLIC
    ${Boost_LIBRARY_DIRS}; 
    ${FairLogger_LIBDIR};
    ${FairMQ_LIBDIR};
)

if ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.8.0)
  AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.8.9))
  target_link_libraries(AmQStrTdcSampler  PUBLIC
    fairmq;
    FairLogger;
    ${Boost_LIBRARIES};
    Utility;
  )
elseif ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.4.55)
  AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.4.56))
  target_link_libraries(AmQStrTdcSampler  PUBLIC
    FairMQ;
    FairMQStateMachine;
    FairLogger;
    ${Boost_LIBRARIES};
    Utility;
  )
else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
endif()


install(TARGETS
    AmQStrTdcSampler
    RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR}
)


#
#
#

foreach(lvar IN ITEMS
    TriggerView;
)

set(EXEC ${lvar})
  add_executable(${EXEC} 
  ${EXEC}.cxx;
)

target_include_directories(${EXEC} PUBLIC 
    ${Boost_INCLUDE_DIRS};  
    ${FairMQ_INCDIR};
    ${FairMQ_INCDIR}/fairmq;
    ${FairMQ_INCDIR}/fairlogger/bundled;
    ${CMAKE_SOURCE_DIR};
    ${ROOT_INCLUDE_DIRS};
    ${HIREDIS_HEADER};
    ${REDIS_PLUS_PLUS_HEADER};
)

target_link_directories(${EXEC} PUBLIC
    ${Boost_LIBRARY_DIRS}; 
    ${FairMQ_LIBDIR};
    ${ROOT_LIBRARIES};
)

if ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.8.0)
  AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.8.9))
  target_link_libraries(${EXEC} PUBLIC 
    ${Boost_LIBRARIES};  
    fairmq
    FairLogger;
    ${ROOT_LIBRARIES};
    ${HIREDIS_LIB};
    ${REDIS_PLUS_PLUS_LIB};
  )
elseif ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.4.55)
  AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.4.56))
  target_link_libraries(${EXEC} PUBLIC 
    ${Boost_LIBRARIES};  
    FairMQ;
    FairMQStateMachine;
    FairLogger;
    ${ROOT_LIBRARIES};
    ${HIREDIS_LIB};
    ${REDIS_PLUS_PLUS_LIB};
  )
else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
endif()

install(TARGETS ${EXEC};
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    RUNTIME DESTINATION ${INSTALL_BINDIR}
)

endforeach()


#================================================================================
add_subdirectory(utility)
add_subdirectory(emulator)
