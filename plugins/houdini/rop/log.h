#ifndef __ROP_GtoExport_log_h__
#define __ROP_GtoExport_log_h__

namespace Log
{
  bool IsEnabled();
  void Enable();
  void Disable();
  void Toggle(bool on);
  
  void Print(const char *fmt, ...);
  void PrintDebug(const char *fmt, ...);
  void PrintError(const char *fmt, ...);
}

#endif
