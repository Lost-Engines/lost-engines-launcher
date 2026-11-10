#include "open_url.h"
#include <iostream>
#include "platform_defines.h"
#if defined(PLATFORM_WINDOWS)
  #include <objbase.h>
#elif defined(PLATFORM_LINUX)
  #include <unistd.h>
#elif defined(PLATFORM_MACOS)
  // nothing to include
#else
  #error "Compilation platform could not be determined.  Make sure platform_defines.h is included and up to date."
#endif // defined

void open_url(std::string const &url) {
  /// Open the specified URL in the system default browser, regardless of platform
  std::cout << "Opening URL " << url << std::endl;
  #if defined(PLATFORM_WINDOWS)
    // see https://msdn.microsoft.com/en-us/library/windows/desktop/bb762153(v=vs.85).aspx
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);    // see http://stackoverflow.com/a/153398/1678468 and https://blogs.msdn.microsoft.com/benkuhn/2006/09/15/handling-coinitialize-and-couninitialize/
    ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOW);               // Activates the window and displays it in its current size and position.
    //ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWDEFAULT);        // Sets the show state based on the SW_ flag specified in the STARTUPINFO structure passed to the CreateProcess function by the program that started the application. An application should call ShowWindow with this flag to set the initial show state of its main window.
    //ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);         // Activates and displays a window. If the window is minimized or maximized, Windows restores it to its original size and position. An application should specify this flag when displaying the window for the first time.
    CoUninitialize();                                                           // see https://blogs.msdn.microsoft.com/benkuhn/2006/09/15/handling-coinitialize-and-couninitialize/
  #elif defined(PLATFORM_LINUX)
    pid_t pid = fork();
    if(!pid) {
      execl("/usr/bin/xdg-open", "xdg-open", url.c_str(), (char*)NULL);
    }
  #elif defined(PLATFORM_MACOS)
    std::string command = "open " + url;
    system(command.c_str());
  #endif // defined
}
