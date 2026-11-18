#==============================================================================

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(INSTALL_BINDIR ${CMAKE_SOURCE_DIR}/bin)

foreach(lvar IN ITEMS
	TriggerView;
)

set(EXEC ${lvar})
  add_executable(${EXEC} 
  ${EXEC}.cxx;
  ${CMAKE_SOURCE_DIR}/LogicFilter/SignalParser.cxx
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
        fairmq;
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
elseif((FairMQ_VERSION VERSION_EQUAL 1.10.0))
    target_link_libraries(${EXEC}  PUBLIC
    fairmq;
    FairLogger;
    ${Boost_LIBRARIES};
    Utility;
    sqlite3;
    ${ROOT_LIBRARIES};
    ${HIREDIS_LIB};
    ${REDIS_PLUS_PLUS_LIB};
    )
else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
endif()


install(TARGETS ${EXEC};
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib64
    ARCHIVE DESTINATION lib64
)

endforeach()
