// detours.h — stand-in. Upstream wraps every hook in a Detours transaction; on Linux
// CoreLib/Wrappers.h installs through loader/src/hook.{h,cpp} at Wrap() time, so the
// transaction calls are no-ops that report success.
#pragma once
#include <windows.h>
inline LONG DetourTransactionBegin() { return NO_ERROR; }
inline LONG DetourTransactionCommit() { return NO_ERROR; }
inline LONG DetourUpdateThread(HANDLE) { return NO_ERROR; }
