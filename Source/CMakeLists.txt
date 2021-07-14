set(SOURCES
	Application.cpp
	Application.h
	Camera.cpp
	Camera.h
	DDSTextureLoader12.cpp
	DDSTextureLoader12.h
	DebugDrawer.cpp
	DebugDrawer.h
	Halton.cpp
	Halton.h
	LightSource.cpp
	LightSource.h
	Main.cpp
	Manager.cpp
	Manager.h
	Mesh.cpp
	Mesh.h
	PCH.h
	PointLight.h
	ProxyLightDrawer.cpp
	ProxyLightDrawer.h
	d3dx12.h
	tiny_obj_loader.cpp
	tiny_obj_loader.h
	RenderGraph/RenderGraph.cpp
	RenderGraph/RenderGraph.h
	)

add_executable(Pool WIN32 ${SOURCES})

source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${SOURCES})

target_compile_features(Pool PUBLIC cxx_std_17)
target_compile_definitions(Pool PUBLIC UNICODE)

# unfortunately, set_target_properties doesn't support generator expressions yet
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIGUPPER)
	if (NOT ${OUTPUTCONFIGUPPER} STREQUAL "RELEASE")
		set_target_properties(Pool PROPERTIES ${OUTPUTCONFIGUPPER}_POSTFIX "_${OUTPUTCONFIG}")
	endif()
endforeach()

set_target_properties(Pool PROPERTIES VS_DPI_AWARE ON)

target_link_libraries(Pool
	d3d12
	dxgi
	dxguid
	WinPixEventRuntime)

target_include_directories(Pool
	PRIVATE .
	PRIVATE ${THIRD_PARTY_DIR}/glm
	PRIVATE ${THIRD_PARTY_DIR}/WinPixEventRuntime/Include
	PRIVATE ${COMPILED_SHADERS_DIR})

target_link_directories(Pool
	PRIVATE ${THIRD_PARTY_DIR}/WinPixEventRuntime/Lib)

target_precompile_headers(Pool PUBLIC PCH.h)

configure_file(${THIRD_PARTY_DIR}/WinPixEventRuntime/Lib/WinPixEventRuntime.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} COPYONLY)


# rendergraph test

set(RENDER_GRAPH_SOURCES
	RenderGraph/RenderGraph.cpp
	RenderGraph/RenderGraph.h
	RenderGraph/RenderGraphTest.cpp
	RenderGraph/RenderGraphTest.h)

add_executable(RenderGraphTest ${RENDER_GRAPH_SOURCES})
target_compile_features(RenderGraphTest PUBLIC cxx_std_17)