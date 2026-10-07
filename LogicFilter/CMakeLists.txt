#==============================================================================

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(INSTALL_BINDIR ${CMAKE_SOURCE_DIR}/bin)
set(INSTALL_LIBDIR ${CMAKE_SOURCE_DIR}/lib)


foreach(lvar IN ITEMS
	LogicFilter
	LogicFilterGpu
	LogicFilterSlice
	fltcoin
)
  set(EXEC ${lvar})
  add_executable(${EXEC} 
    ${EXEC}.cxx;
  )

  # CUDA sources stay off the original LogicFilter / fltcoin targets.
  if((${EXEC} STREQUAL "LogicFilterGpu") AND USE_CUDA)
    target_sources(${EXEC} PRIVATE TriggerCuda.cu)
    target_compile_definitions(${EXEC} PRIVATE USE_CUDA)
    target_link_libraries(${EXEC} PRIVATE CUDA::cudart)
    set_target_properties(${EXEC} PROPERTIES
      CUDA_SEPARABLE_COMPILATION ON
      CUDA_STANDARD 17
      CUDA_STANDARD_REQUIRED ON
      CUDA_ARCHITECTURES native
    )
  endif()

  # LogicFilterSlice keeps its CPU and GPU engine in TriggerSlice.cxx. That file
  # is compiled a second time by nvcc, where __CUDACC__ selects the kernels, so
  # the FairMQ device itself never goes through nvcc. Without USE_CUDA the file
  # is only included by the device and supplies "no GPU" stubs instead.
  if(${EXEC} STREQUAL "LogicFilterSlice")
    target_compile_options(${EXEC} PRIVATE -mavx2)
    target_compile_definitions(${EXEC} PRIVATE TRIGGERSLICE_AVX2)
    if(USE_CUDA)
      set_source_files_properties(TriggerSlice.cxx PROPERTIES LANGUAGE CUDA)
      target_sources(${EXEC} PRIVATE TriggerSlice.cxx)
      target_compile_definitions(${EXEC} PRIVATE USE_CUDA)
      target_link_libraries(${EXEC} PRIVATE CUDA::cudart)
      set_target_properties(${EXEC} PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_STANDARD 17
        CUDA_STANDARD_REQUIRED ON
        CUDA_ARCHITECTURES native
      )
    endif()
  endif()

  target_include_directories(${EXEC} PUBLIC 
    ${Boost_INCLUDE_DIRS};  
    ${FairMQ_INCDIR};
    ${FairMQ_INCDIR}/fairmq;
    ${FairLogger_INCDIR};
    ${CMAKE_SOURCE_DIR};
    ${HIREDIS_HEADER};
    ${REDIS_PLUS_PLUS_HEADER};
    ${UHBOOK_HEADER};
    ./;
  )

  target_link_directories(${EXEC} PUBLIC
    ${Boost_LIBRARY_DIRS}; 
    ${FairMQ_LIBDIR};
  )

  if ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.8.0)
    AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.8.9))
    target_link_libraries(${EXEC} PUBLIC 
      ${Boost_LIBRARIES};  
      fairmq;
      FairLogger;
      ${HIREDIS_LIB};
      ${REDIS_PLUS_PLUS_LIB};
      pthread;
    )
  elseif ((FairMQ_VERSION VERSION_GREATER_EQUAL 1.4.55)
    AND (FairMQ_VERSION VERSION_LESS_EQUAL 1.4.56))
    target_link_libraries(${EXEC} PUBLIC 
      ${Boost_LIBRARIES};  
      FairMQ;
      FairMQStateMachine;
      FairLogger;
      ${HIREDIS_LIB};
      ${REDIS_PLUS_PLUS_LIB};
      pthread;
    )
  else()
    message(FATAL_ERROR "Unsupported FairMQ version ${FairMQ_VERSION}")
  endif()

  install(TARGETS
    ${EXEC};
    #LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${INSTALL_LIBDIR}
    RUNTIME DESTINATION ${INSTALL_BINDIR}
  )
endforeach()
