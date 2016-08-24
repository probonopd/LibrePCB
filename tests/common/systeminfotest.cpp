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
#include <gtest/gtest.h>
#include <librepcbcommon/systeminfo.h>
#include <librepcbcommon/fileio/filepath.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace tests {

/*****************************************************************************************
 *  Test Class
 ****************************************************************************************/

class SystemInfoTest : public ::testing::Test
{
};

/*****************************************************************************************
 *  Test Methods
 ****************************************************************************************/

TEST(SystemInfo, testGetUsername)
{
    // the username must not be empty on any system
    QString username = SystemInfo::getUsername();
    ASSERT_FALSE(username.isEmpty());
    std::cout << "Username: " << qPrintable(username) << std::endl;
}

TEST(SystemInfo, testGetFullUsername)
{
    // the full username may be empty because the user didn't set it...
    QString fullUsername = SystemInfo::getFullUsername();
    std::cout << "Full username: " << qPrintable(fullUsername) << std::endl;
}

TEST(SystemInfo, testGetHostname)
{
    // the hostname must not be empty on any system
    QString hostname = SystemInfo::getHostname();
    ASSERT_FALSE(hostname.isEmpty());
    std::cout << "Hostname: " << qPrintable(hostname) << std::endl;
}

TEST(SystemInfo, testGetProcessStartTime)
{
    // check start time of this process
    {
        qint64 pid = qApp->applicationPid();
        QDateTime startTime = SystemInfo::getProcessStartTime(pid);
        QDateTime currentTime = QDateTime::currentDateTimeUtc();
        qint64 diffMs = currentTime.toMSecsSinceEpoch() - startTime.toMSecsSinceEpoch();
        std::cout << "Time difference [ms]: " << diffMs << std::endl;
        ASSERT_TRUE((diffMs > 0) && (diffMs < 30*60*1000)); // allow up to 30min difference
    }

    // check start time of another process
    {
        FilePath generatedDir(qApp->applicationDirPath());
        FilePath librepcbExe = generatedDir.getPathTo("librepcb");

        QProcess process;
        process.start(librepcbExe.toStr());
        ASSERT_TRUE(process.waitForStarted());

        qint64 pid = process.processId();
        QDateTime startTime = SystemInfo::getProcessStartTime(pid);
        QDateTime currentTime = QDateTime::currentDateTimeUtc();
        qint64 diffMs = currentTime.toMSecsSinceEpoch() - startTime.toMSecsSinceEpoch();
        std::cout << "Time difference [ms]: " << diffMs << std::endl;
        ASSERT_TRUE(abs(diffMs) < 10000); // allow up to 10s difference
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace tests
} // namespace librepcb
