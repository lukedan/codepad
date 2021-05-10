file(
	COPY ${CODEPAD_SKIA_SOURCE_INCLUDE}
	DESTINATION ${CODEPAD_SKIA_BUILD_INCLUDE})

# replace #include "include/.." with #include "skia/.."
file(GLOB_RECURSE HEADERS "${CODEPAD_SKIA_BUILD_INCLUDE}/*")
foreach(HEADER ${HEADERS})
	file(READ "${HEADER}" HEADER_CONTENTS)
	string(REPLACE "#include \"include/" "#include \"skia/" NEW_CONTENTS "${HEADER_CONTENTS}")
	file(WRITE "${HEADER}" "${NEW_CONTENTS}")
endforeach()
