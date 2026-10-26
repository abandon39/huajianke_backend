add_library(pans_options INTERFACE)

target_compile_options(pans_options INTERFACE
	# GCC/Clang 通用选项
	$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
		-Wall
		-Wextra    #额外警告（比-Wall更严格）
		-Wpedantic    #编译过程严格遵循标准
		-fno-strict-aliasing    #关闭严格别名规则，关闭后，不同类型指针可以互相赋值
	>
)

target_compile_options(pans_options INTERFACE
	$<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
		-fPIC    #编译位置无关代码
	>
)

target_link_options(pans_options INTERFACE
	$<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
		-rdynamic    #栈回溯需要
	>
)

target_compile_definitions(pans_options INTERFACE
	$<$<CONFIG:Debug>:PANS_DEBUG>
)

target_compile_options(pans_options INTERFACE
	$<$<CONFIG:Debug>:-O0>    #代码不重新编排，不优化
	$<$<CONFIG:Debug>:-g3>    #生成更完整的调试信息，比-g更详细还有一些调试宏等
	$<$<CONFIG:Debug>:-ggdb>    #生成适用于GDP调试信息的一些格式，偏格式选择
)
# Release/RelWithDebInfo 共用选项
target_compile_options(pans_options INTERFACE
	$<$<CONFIG:Release>:-DNDEBUG>    #noDebug
	$<$<CONFIG:Release>:-O2>    #O2优化级别
	$<$<CONFIG:Release>:-fno-omit-frame-pointer>    #尽可能保留栈指针，好像没什么用

	#Release带上Debug符号
	$<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
	$<$<CONFIG:RelWithDebInfo>:-O2>
	$<$<CONFIG:RelWithDebInfo>:-g>    #带上编译符号
	$<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
)

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
	target_compile_options(pans_options INTERFACE
		$<$<CONFIG:Debug>:--coverage>
	)
	target_link_options(pans_options INTERFACE
		$<$<CONFIG:Debug>:--coverage>
	)
endif()

