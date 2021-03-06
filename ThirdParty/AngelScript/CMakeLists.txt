# Define target name
set (TARGET_NAME AngelScript)

# Define source files
file (GLOB CPP_FILES
    source/*.cpp
)
file (GLOB H_FILES
    include/*.h source/*.h
)

if (IOS)
    set (ASM_FILES source/as_callfunc_arm_xcode.S)
endif ()

if (MSVC AND ENABLE_64BIT)
    # Commented out due to cmake bug
    #enable_language(ASM_MASM)
    #set (ASM_FILES source/as_callfunc_x64_msvc_asm.asm)
    # As a temporary workaround
    add_custom_command(OUTPUT as_callfunc_x64_msvc_asm.obj COMMAND ${CMAKE_ASM_MASM_COMPILER} -c source/as_callfunc_x64_msvc_asm.asm DEPENDS source/as_callfunc_x64_msvc_asm.asm COMMENT "Temporary workaround for MASM") 
    set (ASM_FILES as_callfunc_x64_msvc_asm.obj source/as_callfunc_x64_msvc_asm.asm)
endif ()

set (SOURCE_FILES ${CPP_FILES} ${ASM_FILES} ${H_FILES})

# Define dependency libs
set (INCLUDE_DIRS_ONLY include source)

# Setup target
setup_library ()

