#include "fileUtils.h"
#include <algorithm>
#include <cstdio>

#if (defined(__GNUC__) || defined(__GCCXML__)) && !defined(_stricmp)
#include <string.h>
#define _stricmp strcasecmp
#endif

#include <sys/stat.h>

bool FileExists(const std::string &path)
{
  struct stat st;
  return (stat(path.c_str(), &st) == 0);
}

size_t ListDirectoryFiles(const std::string &path, bool (*filter)(const std::string&, void*),
                          void *userData, std::vector<std::string> &files)
{
  files.clear();

#ifdef _WIN32

  WIN32_FIND_DATAA findData;
  memset(&findData, 0, sizeof(findData));

  std::string baseName, basePath, searchPath;

  basePath = path;
  size_t len = basePath.length();
  if (len == 0)
  {
    return 0;
  }
  if (basePath[len-1] != '\\' && basePath[len-1] != '/')
  {
    basePath += "\\";
  }

  searchPath = basePath + "*";

  HANDLE hFile = ::FindFirstFileA(searchPath.c_str(), &findData);
  if (hFile  == INVALID_HANDLE_VALUE)
  {
    return 0;
  }

  do
  {

    if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {

      baseName = findData.cFileName;

      if (baseName != "." && baseName != "..")
      {
        if (filter(baseName, userData))
        {
          files.push_back(basePath + baseName);
        }
      }
    }
    
  } while (::FindNextFileA(hFile, &findData));

  ::FindClose(hFile);

#else
  std::string basePath(path);
  size_t len = basePath.length();
  if (len == 0)
  {
    return 0;
  }
  if (basePath[len-1] != '/')
  {
    basePath += "/";
  }
  
  DIR *dp = opendir(basePath.c_str());
  if (dp != NULL)
  {
    struct dirent *ep;
    while (ep = readdir(dp))
    {
      if (ep->d_type == DT_REG)
      {
        // regular file
        std::string baseName(ep->d_name);
        if (filter(baseName, userData))
        {
          files.push_back(basePath + baseName);
        }
      }
    }//while

    closedir(dp);
  }
  else
  {
    // couldn't open the directory
    return 0;
  }
#endif

  return files.size();
}

bool GetGtoFrameAndSubFrame(const std::string &file, int &frame, int &subFrame, std::string *basePath)
{
  size_t p0, p1, p2;
  bool hassubframe = false;

  p0 = file.rfind('.');
  if (p0 == std::string::npos)
  {
    return false;
  }

  p1 = file.rfind('.', p0-1);
  if (p1 == std::string::npos)
  {
    return false;
  }

  p2 = file.rfind('.', p1-1);
  if (p2 != std::string::npos)
  {
    hassubframe = true;
  }

  std::string sstr;
  
  sstr = file.substr(p0+1);
  if (_stricmp(sstr.c_str(), "gto"))
  {
    return false;
  }

  sstr = file.substr(p1+1, p0-p1-1);
  if (sscanf(sstr.c_str(), "%d", &subFrame) != 1)
  {
    return false;
  }

  if (!hassubframe)
  {
    frame = subFrame;
    subFrame = 0;
  }
  else
  {
    sstr = file.substr(p2+1, p1-p2-1);
    if (sscanf(sstr.c_str(), "%d", &frame) != 1)
    {
      return false;
    }
  }
  
  if (basePath != 0)
  {
    *basePath = file.substr(0, (hassubframe ? p2 : p1));
  }
  
  return true;
}

struct Filter_FrameFile_Data
{
  std::vector<std::pair<int, int> > allFrames;
  std::string baseName;
};

bool Filter_FrameFile(const std::string &file, void *userData)
{
  
  int ff, sf;

  Filter_FrameFile_Data *data = (Filter_FrameFile_Data*)userData;

  size_t p = file.find(data->baseName);

  if (p == 0)
  {

    std::string ext = file.substr(data->baseName.length());

    // strlen(".0000.000.gto") == 13
    // strlen(".0000.gto") == 9
    if (ext[0] == '.' && (ext.length() == 13 || ext.length() == 9))
    {

      if (GetGtoFrameAndSubFrame(file, ff, sf))
      {
        data->allFrames.push_back(std::pair<int, int>(ff, sf));
        return true;

      }
    }
  }

  return false;
}



float GetPreviousAndNextFrame(const std::string &fileName, std::string &prev, std::string &next, float *prevFrame, float *nextFrame)
{
  
  //std::vector<std::pair<int, int> > allFrames;
  Filter_FrameFile_Data userData;
  std::vector<std::string> allFiles;

  std::pair<int, int> targetFrame;

  std::string filePath = fileName;
  
  size_t p = filePath.find_last_of("\\/");

  if (p == std::string::npos)
  {
    userData.baseName = filePath;
    filePath = ".";
  }
  else
  {
    userData.baseName = filePath.substr(p+1);
    filePath = filePath.substr(0, p);
  }
  
  if (!GetGtoFrameAndSubFrame(userData.baseName, targetFrame.first, targetFrame.second, &(userData.baseName)))
  {
    p = userData.baseName.rfind('.');
    if (p != std::string::npos && !_stricmp(userData.baseName.substr(p+1).c_str(), "gto"))
    {
      userData.baseName = userData.baseName.substr(0, p);
    }
  }
  
  ListDirectoryFiles(filePath, Filter_FrameFile, (void*)&userData, allFiles);
  
  userData.allFrames.push_back(targetFrame);

  std::sort(userData.allFrames.begin(), userData.allFrames.end());
  std::sort(allFiles.begin(), allFiles.end());

  std::vector<std::pair<int, int> >::iterator it = std::find(userData.allFrames.begin(), userData.allFrames.end(), targetFrame);
  
  size_t idx = it - userData.allFrames.begin();
  
  float prevTime = 0.0f;
  float nextTime = 0.0f;
  float curTime = float(targetFrame.first) + 0.001f * float(targetFrame.second);

  if (idx > 0)
  {
    prev = allFiles[idx-1];
    prevTime = float(userData.allFrames[idx-1].first) + 0.001f * float(userData.allFrames[idx-1].second);

  }
  else
  {
    prevTime = curTime;
  }

  if (idx < allFiles.size())
  {
    next = allFiles[idx];
    nextTime = float(userData.allFrames[idx+1].first) + 0.001f * float(userData.allFrames[idx+1].second);

  }
  else
  {
    nextTime = curTime;
  }

  if (prevFrame)
  {
    *prevFrame = prevTime;
  }
  if (nextFrame)
  {
    *nextFrame = nextTime;
  }

  if (nextTime != prevTime)
  {
    return (curTime - prevTime) / (nextTime - prevTime);
  }
  else
  {
    return 0.0f;
  }
}

