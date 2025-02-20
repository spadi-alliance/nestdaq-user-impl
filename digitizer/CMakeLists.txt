#=========== for LIB ===============
add_library(digitizer
  fft.c;
  flash.c;
  spi.c;
  X742CorrectionRoutines.c;
)

target_include_directories(digitizer PUBLIC
  ${CMAKE_SOURCE_DIRS};
  /usr/include
)

#========== install ===========
install(TARGETS 
  digitizer
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
