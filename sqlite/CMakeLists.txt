#========== sqlite amalgamation C-source ============
add_executable(sqlite3a
  shell.c;
  sqlite3.c;
)

target_include_directories(sqlite3a PUBLIC
  ${CMAKE_SOURCE_DIRS};
)
target_link_libraries(sqlite3a PRIVATE
  ${CMAKE_DL_LIBS};
  pthread;
)

#=========== for LIB ===============
add_library(sqlite3
  sqlite3.c;
)

target_include_directories(sqlite3 PUBLIC
  ${CMAKE_SOURCE_DIRS};
)
target_link_libraries(sqlite3 PRIVATE
  ${CMAKE_DL_LIBS};
  pthread;
)


#========== install ===========
install(TARGETS sqlite3a
  sqlite3;
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)


