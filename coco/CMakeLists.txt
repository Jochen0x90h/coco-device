add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
	PUBLIC FILE_SET headers TYPE HEADERS FILES
		Buffer.hpp
		BufferImpl.hpp
		BufferReader.hpp
		BufferWriter.hpp
		DataBuffer.hpp
		Device.hpp
		BufferDevice.hpp
	PRIVATE
		BufferImpl.cpp
		Device.cpp
)

target_link_libraries(${PROJECT_NAME}
	coco::coco
)

target_include_directories(${PROJECT_NAME}
	PUBLIC
		..
)

# install the library
install(TARGETS ${PROJECT_NAME}
	FILE_SET headers DESTINATION include/coco
	FILE_SET platform_headers DESTINATION include
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
