
#pragma once

#include <string>

namespace open_rtc {

// A callback function to run after the minidump has been written.
// |dump_file_path| is the path of the minidump file.
// |context| is the value passed into the constructor.
// |succeeded| indicates whether a minidump file was successfully written.
// Return true if the exception was fully handled and breakpad should exit.
// Return false to allow any other exception handlers to process the
// exception.
typedef bool (*CrashDumpCallback)(const char* dump_file_path,
                                  void* context,
                                  bool succeeded);

void InstallCrashHandler();

bool ConfigCrashDumpPath(const char* dump_path,
                         CrashDumpCallback callback,
                         void* callback_context);

void UnInstallCrashHandler();

}  // namespace open_rtc
