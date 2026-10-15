zephyr_library_amend()
set_source_files_properties(
        ${ZEPHYR_BASE}/drivers/input/input_cst816s.c
        TARGET_DIRECTORY ${lib_name}
        PROPERTIES HEADER_FILE_ONLY ON)
zephyr_library_sources(input_cst816s.c)
