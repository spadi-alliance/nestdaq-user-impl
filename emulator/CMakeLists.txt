#==============================================
add_executable(emulator
	Emulator.cxx;
	AmQLRTdcData.cxx;
	)

add_library(AmQLRTdcData
	AmQLRTdcData.cxx;
	)
#==============================================
install(TARGETS emulator 
	AmQLRTdcData;
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	RUNTIME DESTINATION ./emulator/bin
)
