#ifndef COMMON_H
#define COMMON_H

#include <string>

std::string create_temporary_dbc_with(const char* contents);

// Same as create_temporary_dbc_with, but places the file inside a subdirectory
// of the temp dir named dir_name_utf8 (a UTF-8 encoded string, may contain
// non-ASCII characters such as Chinese). The returned path is UTF-8 encoded.
std::string create_temporary_dbc_in_dir_with(const std::string& dir_name_utf8, const char* contents);

#endif // COMMON_H
