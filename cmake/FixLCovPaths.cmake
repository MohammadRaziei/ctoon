# Read the content of the coverage file
file(READ ${COV_FILE} CONTENT)

# Replace the absolute source path with an empty string
# To relative paths like: include/...
string(REPLACE "SF:${SOURCE_PATH}" "SF:${PREFIX_PATH}" CONTENT "${CONTENT}")

# Write the modified content back to the file
file(WRITE ${COV_FILE} "${CONTENT}")
