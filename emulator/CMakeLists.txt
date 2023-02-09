#==============================================
add_executable(emulator
	Emulator.cxx;
	AmQTdc.cxx;
	)

add_library(AmQTdcModule
	AmQTdc.cxx;
	)
#==============================================
install(TARGETS emulator 
	AmQTdcModule;
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	RUNTIME DESTINATION ./emulator/bin
)
