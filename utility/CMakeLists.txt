#_______________________________________________________________________________
set(SimpleLibList
  Stoi;
)

foreach(lib IN LISTS SimpleLibList)
  message(STATUS "lib: ${lib}")
  add_library(${lib} ${lib}.cxx)
  target_include_directories(${lib} PRIVATE ${CMAKE_SOURCE_DIR})
endforeach()

#_______________________________________________________________________________
add_library(program_options
  program_options.cxx
)

target_include_directories(program_options PRIVATE
  ${CMAKE_SOURCE_DIR};
  ${Boost_INCLUDE_DIRS};
)

#_______________________________________________________________________________
#add_library(RegisterMap
#  RegisterMap.cxx;
#  RegisterMapParser.cxx;
#)

#target_include_directories(RegisterMap PRIVATE
#  ${CMAKE_SOURCE_DIR};
#  ${YAML_CPP_INCLUDE_DIR};
#  ${Boost_INCLUDE_DIRS};
#)

#_______________________________________________________________________________
#if(ROOT_FOUND)
#  include_directories(${CMAKE_SOURCE_DIR})
#  ROOT_GENERATE_DICTIONARY(G__Options
#    utility/is_vector.h;
#    utility/Options.h;
#    LINKDEF Options_LinkDef.h
#    )
#
#  add_library(Options SHARED
#    Options.cxx;
#    G__Options.cxx;
#    )
#
#  install(FILES
#    ${CMAKE_CURRENT_BINARY_DIR}/libOptions_rdict.pcm
#    DESTINATION ${CMAKE_INSTALL_LIBDIR}
#    )
#  set(EXTRA_INC ${ROOT_INCLUDE_DIRS})
#  set(EXTRA_LIB ${ROOT_LIBRARIES})
#else()
#  add_library(Options
#    Options.cxx
#    )
#endif()

#target_include_directories(Options PRIVATE
#  ${EXTRA_INC};
#  ${YAML_CPP_INCLUDE_DIR};
#  ${CMAKE_SOURCE_DIR};
#)

#target_link_libraries(Options
#  ${EXTRA_LIB};
#  ${YAML_CPP_LIBRARIES}
#)

#_______________________________________________________________________________
add_library(Utility
  Stoi.cxx;
#  Options.cxx;
  FileUtil.cxx;
  program_options.cxx;
#  RegisterMap.cxx;
#  RegisterMapParser.cxx;
  Timer.cxx;
  TaskProcessorMT.cxx
  Reporter.cxx;
  UDPRBCP.cc;
  BitDump.cc;
  )

if(USE_LZMA)
  add_compile_definitions(Utility USE_LZMA=1)
endif()

target_include_directories(Utility PRIVATE
  ${Boost_INCLUDE_DIRS};
  ${FairLogger_INCDIR};
  ${FairMQ_INCDIR};
  ${YAML_CPP_INCLUDE_DIR};
  ${CMAKE_SOURCE_DIR};
)

target_link_directories(Utility PRIVATE
  ${Boost_LIBRARY_DIRS};
)

target_link_libraries(Utility
  ${Boost_LIBRARIES};
  ${YAML_CPP_LIBRARIES};
  ${fmt_LIB};
#  ${FairLogger_LIBDIR};
#  ${FairMQ_LIBDIR};
)


#_______________________________________________________________________________
install(TARGETS
  ${SimpleLibList};
  program_options;
#  RegisterMap;
#  Options;
  Utility;
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
