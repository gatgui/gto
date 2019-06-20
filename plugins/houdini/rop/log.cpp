#include "log.h"
#include <fstream>
#include <cstdlib>
#include <cstdarg>

namespace Log
{

#ifdef _DEBUG
bool gLogEnable = true;
#else
bool gLogEnable = false;
#endif
std::ofstream gLog;

static bool EnsureLog()
{
  if (gLogEnable)
  {
    if (!gLog.is_open())
    {
      std::string path;
#ifdef _WIN32
      char *appdata = getenv("APPDATA"  );
      path = (appdata ? appdata : ".");
#else
      char *home = getenv("HOME");
      path = (home ? home : ".");
#endif
      path += "/ROP_GtoExport.log";
      gLog.open(path.c_str(), std::ios_base::out|std::ios_base::app);
    }
    return gLog.is_open();
  }
  return false;
}

bool IsEnabled()
{
  return gLogEnable;
}

void Enable()
{
  gLogEnable = true;
  EnsureLog();
}

void Disable()
{
  gLogEnable = false;
  if (gLog.is_open())
  {
    gLog.close();
  }
}

void Toggle(bool on)
{
  if (on)
  {
    Enable();
  }
  else
  {
    Disable();
  }
}

void Print(const char *fmt, ...)
{
  if (!EnsureLog())
  {
    return;
  }
  char tmp[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, 1024, fmt, args);
  va_end(args);
  gLog << "[Message] " << tmp << std::endl;
  gLog.flush();
}

void PrintError(const char *fmt, ...)
{
  if (!EnsureLog())
  {
    return;
  }
  char tmp[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, 1024, fmt, args);
  va_end(args);
  gLog << "[Debug] " << tmp << std::endl;
  gLog.flush();
}

#ifdef _DEBUG
void PrintDebug(const char *fmt, ...)
{
  if (!EnsureLog())
  {
    return;
  }
  char tmp[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, 1024, fmt, args);
  va_end(args);
  gLog << "[Debug] " << tmp << std::endl;
  gLog.flush();
}
#else
void PrintDebug(const char *, ...)
{
}
#endif

}
