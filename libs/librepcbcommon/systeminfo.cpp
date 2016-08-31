/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * http://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include <QHostInfo>
#include "systeminfo.h"
#include "fileio/filepath.h"

#if defined(Q_OS_OSX) // Mac OS X
#include <cerrno>
#include <system_error>
#include <sys/types.h>
#include <signal.h>
#include <libproc.h>
#elif defined(Q_OS_UNIX) // UNIX/Linux
#include <cerrno>
#include <system_error>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#elif defined(Q_OS_WIN32) || defined(Q_OS_WIN64) // Windows
#include <Windows.h>
#else
#error "Unknown operating system!"
#endif

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

/*****************************************************************************************
 *  Static Methods
 ****************************************************************************************/

QString SystemInfo::getUsername() noexcept
{
    QString username("");

    // this line should work for most UNIX, Linux, Mac and Windows systems
    username = QString(qgetenv("USERNAME")).trimmed();

    // if the environment variable "USERNAME" is not set, we will try "USER"
    if (username.isEmpty())
        username = QString(qgetenv("USER")).trimmed();

    if (username.isEmpty())
        qWarning() << "Could not determine the system's username!";

    return username;
}

QString SystemInfo::getFullUsername() noexcept
{
    QString username("");

#if (defined(Q_OS_UNIX) || defined(Q_OS_LINUX)) && (!defined(Q_OS_MACX)) // For UNIX and Linux
    passwd* userinfo = getpwuid(getuid());
    if (userinfo == NULL) {
        qWarning() << "Could not fetch user info via getpwuid!";
    } else {
        QString gecosString = QString::fromLocal8Bit(userinfo->pw_gecos);
        QStringList gecosParts = gecosString.split(',', QString::SkipEmptyParts);
        if (gecosParts.size() >= 1) {
            username = gecosParts.at(0);
        }
    }
#elif defined(Q_OS_MACX) // For Mac OS X
    QString command("finger `whoami` | awk -F: '{ print $3 }' | head -n1 | sed 's/^ //'");
    QProcess process;
    process.start("sh", QStringList() << "-c" << command);
    process.waitForFinished(500);
    username = QString(process.readAllStandardOutput()).remove("\n").remove("\r").trimmed();
#elif defined(Q_OS_WIN) // For Windows
    // TODO
#else
#error Unknown operating system!
#endif

    if (username.isEmpty())
        qWarning() << "Could not determine the system's full username!";

    return username;
}

QString SystemInfo::getHostname() noexcept
{
    QString hostname = QHostInfo::localHostName();

    if (hostname.isEmpty())
        qWarning() << "Could not determine the system's hostname!";

    return hostname;
}

bool SystemInfo::isProcessRunning(qint64 pid) throw (Exception)
{
#if defined(Q_OS_UNIX) // Mac OS X / Linux / UNIX
    // From: http://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/io/qlockfile_unix.cpp
    errno = 0;
    int ret = ::kill(pid, 0);
    if (ret == 0) {
        return true;
    } else if ((ret == -1) && (errno == static_cast<int>(std::errc::no_such_process))) {
        return false;
    } else {
        throw RuntimeError(__FILE__, __LINE__, QString::number(errno),
            tr("Could not determine if another process is running."));
    }
#elif defined(Q_OS_WIN32) || defined(Q_OS_WIN64) // Windows
    // From: http://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/io/qlockfile_win.cpp
    HANDLE handle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (handle) {
        DWORD exitCode = 0;
        BOOL success = ::GetExitCodeProcess(handle, &exitCode);
        ::CloseHandle(handle);
        if ((success) && (exitCode == STILL_ACTIVE)) {
            return true;
        } else if (success) {
            return false;
        } else {
            throw RuntimeError(__FILE__, __LINE__, QString::number(GetLastError()),
                tr("Could not determine if another process is running."));
        }
    } else if (GetLastError() == ERROR_INVALID_PARAMETER) {
        return false;
    } else {
        throw RuntimeError(__FILE__, __LINE__, QString::number(GetLastError()),
            tr("Could not determine if another process is running."));
    }
#else
#error "Unknown operating system!"
#endif
}

QString SystemInfo::getProcessNameByPid(qint64 pid) throw (Exception)
{
    QString processName;
#if defined(Q_OS_OSX) // Mac OS X
    // From: http://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/io/qlockfile_unix.cpp
    errno = 0;
    char name[1024] = {0};
    int retval = proc_name(pid, name, sizeof(name) / sizeof(char));
    if (retval > 0) {
        processName = QFile::decodeName(name);
    } else if ((retval == 0) && (errno == static_cast<int>(std::errc::no_such_process))) {
        return QString(); // process not running
    } else {
        throw RuntimeError(__FILE__, __LINE__, QString(),
            QString(tr("proc_name() failed with error %1.")).arg(errno));
    }
#elif defined(Q_OS_UNIX) // UNIX/Linux
    // From: http://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/io/qlockfile_unix.cpp
    if (!FilePath("/proc/version").isExistingFile()) {
        throw RuntimeError(__FILE__, __LINE__, QString(),
            tr("Could not find the file \"/proc/version\"."));
    }
    char exePath[64];
    char buf[PATH_MAX + 1];
    sprintf(exePath, "/proc/%lld/exe", pid);
    size_t len = (size_t)readlink(exePath, buf, sizeof(buf));
    if (len >= sizeof(buf)) {
        return QString(); // process not running
    }
    buf[len] = 0;
    processName = QFileInfo(QFile::decodeName(buf)).fileName();
    // If the executable does no longer exist, the string " (deleted)" is added to the
    // end of the symlink, so we need to remove that to get the naked process name.
    if (processName.endsWith(" (deleted)")) processName.chop(strlen(" (deleted)"));
#elif defined(Q_OS_WIN32) || defined(Q_OS_WIN64) // Windows
    // From: http://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/io/qlockfile_win.cpp
    typedef DWORD (WINAPI *GetModuleFileNameExFunc)(HANDLE, HMODULE, LPTSTR, DWORD);
    HMODULE hPsapi = LoadLibraryA("psapi");
    if (!hPsapi) {
        throw RuntimeError(__FILE__, __LINE__, QString::number(GetLastError()),
            tr("Could not load library 'psapi'."));
    }
    GetModuleFileNameExFunc qGetModuleFileNameEx
            = (GetModuleFileNameExFunc)GetProcAddress(hPsapi, "GetModuleFileNameExW");
    if (!qGetModuleFileNameEx) {
        FreeLibrary(hPsapi);
        throw RuntimeError(__FILE__, __LINE__, QString::number(GetLastError()),
            tr("Function 'GetModuleFileNameExW' not found."));
    }
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, DWORD(pid));
    if (!hProcess) {
        FreeLibrary(hPsapi);
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            return QString(); // process not running
        } else {
            throw RuntimeError(__FILE__, __LINE__, QString(),
                QString(tr("OpenProcess() failed with error %1.")).arg(GetLastError()));
        }
    }
    wchar_t buf[MAX_PATH];
    const DWORD length = qGetModuleFileNameEx(hProcess, NULL, buf, sizeof(buf) / sizeof(wchar_t));
    CloseHandle(hProcess);
    FreeLibrary(hPsapi);
    if (!length) {
        throw RuntimeError(__FILE__, __LINE__, QString(),
            QString(tr("qGetModuleFileNameEx() failed with error %1.")).arg(GetLastError()));
    }
    processName = QString::fromWCharArray(buf, length);
    int i = processName.lastIndexOf(QLatin1Char('\\'));
    if (i >= 0) processName.remove(0, i + 1);
    i = processName.lastIndexOf(QLatin1Char('.'));
    if (i >= 0) processName.truncate(i);
#else
#error "Unknown operating system!"
#endif

    // check if the process name is not empty
    if (processName.isEmpty()) {
        throw RuntimeError(__FILE__, __LINE__, QString(),
            tr("Could not determine the process name of another process."));
    }
    return processName;
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
