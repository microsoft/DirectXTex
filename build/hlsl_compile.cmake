# Authored by: J. Peter Mugaas
# Released to the public domain.  Share and enjoy.
#
# This script compiles a .hlsl to an .inc file using a Wine-compatible (fxc2) program available in MSYS2.  
# The program home page is https://github.com/mozilla/fxc2/ .  This script is intended to be executing
# within the MSYS2 environment

# The parameters are:
#
# WORK_DIR: The current directory where fxc2 is launched.
# INPUT_FILENAME: The input file name WITH the file name extension.
# OUTPUT_FILENAME: The include to write WITHOUT the file name extension (-Fh parameter).
# ENTRY_POINT: The entry-point in the .hlsl source-code (-E parameter)
# VAR_NAME: The variable name for the compiled .inc (-vN parameter)
# SEMANTIC: The semantic or shader model to use (-T parameter).

#first, try Microsoft's FXC syntax
execute_process(
    COMMAND fxc /nologo /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug
        /E ${ENTRY_POINT} /T ${SEMANTIC} /Vn${VAR_NAME} 
        -Fh${OUTPUT_FILENAME}.inc /Fd${OUTPUT_FILENAME}_${ENTRY_POINT}.pdb
		${INPUT_FILENAME}
        WORKING_DIRECTORY "${WORK_DIR}"
        OUTPUT_VARIABLE _STDOUT
		ERROR_VARIABLE _STDERROR
        RESULT_VARIABLE _RES)
if(NOT(${_RES} EQUAL 0))
    #Now try fxc2's syntax
    execute_process(
        COMMAND fxc -E ${ENTRY_POINT} -T ${SEMANTIC} -Vn${VAR_NAME} 
        -Fh${OUTPUT_FILENAME}.inc ${INPUT_FILENAME}
        WORKING_DIRECTORY "${WORK_DIR}"
        OUTPUT_VARIABLE _STDOUT
		ERROR_VARIABLE _STDERROR
        RESULT_VARIABLE _RES)
    #fpr some reason, the -Vn parameter did not work.
    file(READ ${WORK_DIR}/${OUTPUT_FILENAME}.inc BUFFER)
    string(REPLACE "${ENTRY_POINT}" "${VAR_NAME}" BUFFER "${BUFFER}")
    string(REPLACE "}" "}\;" BUFFER "${BUFFER}")
    file(WRITE ${WORK_DIR}/${OUTPUT_FILENAME}.inc ${BUFFER})		
    if(NOT(${_RES} EQUAL 0))
        message(FATAL_ERROR ${_STDOUT})
	endif()
endif()

