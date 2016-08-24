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
#include <type_traits>
#include <sys/proc_info.h>
#include <libproc.h>
#elif defined(Q_OS_UNIX) // UNIX/Linux
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

QDateTime SystemInfo::getProcessStartTime(qint64 pid) throw (Exception)
{
#if defined(Q_OS_OSX) // Mac OS X
    // http://stackoverflow.com/questions/31603885/get-process-creation-date-time-in-osx-with-c-c/31605649#31605649
    // http://opensource.apple.com//source/xnu/xnu-2782.40.9/bsd/kern/proc_info.c
    proc_bsdinfo bsdinfo;
    int ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
    if (ret != 0) {
        throw RuntimeError(__FILE__, __LINE__, QString(),
            tr("Could not determine process start time."));
    }
    static_assert(std::is_same<decltype(bsdinfo.pbi_start_tvsec), quint64>::value, "Unexpected type!");
    throw Exception(__FILE__, __LINE__, QString(), QString::number(bsdinfo.pbi_start_tvsec));
#elif defined(Q_OS_UNIX) // UNIX/Linux
    if (!FilePath("/proc/version").isExistingFile()) {
        throw RuntimeError(__FILE__, __LINE__, QString(),
            tr("Could not find the file \"/proc/version\"."));
    }
    FilePath procDir(QString("/proc/%1").arg(QString::number(pid)));
    if (procDir.isExistingDir()) {
        QFileInfo info(procDir.toStr());
        QDateTime created = info.created().toUTC();
        if (created.isValid()) {
            return created;
        } else {
            throw RuntimeError(__FILE__, __LINE__, QString(), QString(tr(
                "Could not read creation datetime of \"%1\".")).arg(procDir.toNative()));
        }
    } else {
        return QDateTime(); // process is not running
    }
#elif defined(Q_OS_WIN32) || defined(Q_OS_WIN64) // Windows
    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process != NULL) {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        BOOL ret = ::GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime);
        if (ret != FALSE) {
            SYSTEMTIME sysTime;
            BOOL ret = ::FileTimeToSystemTime(&creationTime, &sysTime);
            if (ret != FALSE) {
                QDate date(sysTime.wYear, sysTime.wMonth, sysTime.wDay);
                QTime time(sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
                QDateTime created = QDateTime(date, time, Qt::UTC).toUTC();
                if (created.isValid()) {
                    return created;
                } else {
                    throw RuntimeError(__FILE__, __LINE__, QString(),
                        tr("Could not determine process start time."));
                }
            } else {
                throw RuntimeError(__FILE__, __LINE__, QString(),
                    tr("Could not determine process start time."));
            }
        } else {
            throw RuntimeError(__FILE__, __LINE__, QString(),
                tr("Could not determine process start time."));
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_INVALID_PARAMETER) {
            return QDateTime(); // process is not running
        } else {
            throw RuntimeError(__FILE__, __LINE__, QString(),
                tr("Could not determine process start time."));
        }
    }
#else
#error "Unknown operating system!"
#endif
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
