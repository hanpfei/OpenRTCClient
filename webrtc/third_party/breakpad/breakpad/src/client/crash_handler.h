
#pragma once

#include <string>

namespace open_rtc {

// A callback function to run before Breakpad performs any substantial
// processing of an exception.  A FilterCallback is called before writing
// a minidump.  context is the parameter supplied by the user as
// callback_context when the handler was created.
//
// If a FilterCallback returns true, Breakpad will continue processing,
// attempting to write a minidump.  If a FilterCallback returns false, Breakpad
// will immediately report the exception as unhandled without writing a
// minidump, allowing another handler the opportunity to handle it.
typedef bool (*FilterCallback)(void* context);

// A callback function to run after the minidump has been written.
// |minidump_id| is a unique id for the dump, so the minidump
// file is <dump_dir>/<minidump_id>.dmp.
// |context| is the value passed into the constructor.
// |succeeded| indicates whether a minidump file was successfully written.
// Return true if the exception was fully handled and breakpad should exit.
// Return false to allow any other exception handlers to process the
// exception.
typedef bool (*MinidumpCallback)(const char* dump_dir,
                                 const char* minidump_id,
                                 void* context,
                                 bool succeeded);

void InstallCrashHandler(const std::string& dump_path,
                         FilterCallback filter,
                         MinidumpCallback callback,
                         void* callback_context);

void UnInstallCrashHandler();

}  // namespace sszrtc
