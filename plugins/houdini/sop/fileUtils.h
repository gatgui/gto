#ifndef __fileUtils_h_
#define __fileUtils_h_

#include <string>
#include <vector>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
// TODO
#include <dirent.h>
#endif

bool FileExists(const std::string &path);

size_t ListDirectoryFiles(const std::string &path, bool (*filter)(const std::string&, void*), void *userData, std::vector<std::string> &files);

bool GetGtoFrameAndSubFrame(const std::string &file, int &frame, int &subFrame, std::string *basePath=0);

float GetPreviousAndNextFrame(const std::string &fileName, std::string &prev, std::string &next, float *prevFrame=0, float *nextFrame=0);


#endif
