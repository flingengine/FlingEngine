# Just a small macro that will make a CMake project option, and 
# then add a #define associated to that set to the value of 0 or 1
# depending on if the option is on or not
MACRO( FLING_Compiler_Flag FlagName DefineName Description DefaultValue )

    OPTION( ${FlagName} ${Description} ${DefaultValue} ) 

    # If the option is enabled, then define the value to 1
    IF( ${FlagName} )
        add_compile_definitions( ${DefineName}=1)
        message( Status "${DefineName}=1" )
    # otherwise, set it to false
    else()
        add_compile_definitions( ${DefineName}=0)
        message( Status "${DefineName}=0" )  
    endif()

ENDMACRO(FLING_Compiler_Flag)
