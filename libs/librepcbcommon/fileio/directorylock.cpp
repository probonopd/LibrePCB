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
#include "directorylock.h"
#include "fileutils.h"
#include "../systeminfo.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

DirectoryLock::DirectoryLock() noexcept :
    mDirToLock(), mLockFilePath(), mLockedByThisObject(false)
{
    // nothing to do...
}

DirectoryLock::DirectoryLock(const FilePath& dir) noexcept :
    mDirToLock(), mLockFilePath(), mLockedByThisObject(false)
{
    setDirToLock(dir);
}

DirectoryLock::~DirectoryLock() noexcept
{
    if (mLockedByThisObject) {
        try {
            unlock(); // can throw
        } catch (const Exception& e) {
            qCritical() << "Could not remove lock file:" << e.getUserMsg();
        }
    }
}

/*****************************************************************************************
 *  Setters
 ****************************************************************************************/

void DirectoryLock::setDirToLock(const FilePath& dir) noexcept
{
    Q_ASSERT(!mLockedByThisObject);
    mDirToLock = dir;
    mLockFilePath = dir.getPathTo(".lock");
}

/*****************************************************************************************
 *  Getters
 ****************************************************************************************/

DirectoryLock::LockStatus DirectoryLock::getStatus() const throw (Exception)
{
    // check if the directory to lock does exist
    if (!mDirToLock.isExistingDir()) {
        throw RuntimeError(__FILE__, __LINE__, QString(),  QString(
            tr("The directory \"%1\" does not exist.")).arg(mDirToLock.toNative()));
    }

    // when the directory is valid, the lock filepath must be valid too
    Q_ASSERT(mLockFilePath.isValid());

    // check if the lock file exists
    if (!mLockFilePath.isExistingFile()) {
        return LockStatus::Unlocked;
    }

    // read the content of the lock file
    QString content = QString::fromUtf8(FileUtils::readFile(mLockFilePath)); // can throw
    QStringList lines = content.split("\n", QString::KeepEmptyParts);
    // check count of lines
    if (lines.count() < 5) {
        throw RuntimeError(__FILE__, __LINE__, content, QString(tr(
            "The lock file \"%1\" has too few lines.")).arg(mLockFilePath.toNative()));
    }
    // read lock metadata
    QString lockUser = lines.at(1);
    QString lockHost = lines.at(2);
    qint64 lockPid = lines.at(3).toLongLong();

    // read metadata about this application instance
    QString thisUser = SystemInfo::getUsername().remove("\n");
    QString thisHost = SystemInfo::getHostname().remove("\n");
    qint64 thisPid = QCoreApplication::applicationPid();

    // check if the lock is a non-stale lock
    if ((lockUser != thisUser) || (lockHost != thisHost) || (lockPid == thisPid)) {
        return LockStatus::Locked;
    }

    // the lock was created with another PID --> determine whether it's a stale lock or not
    // @todo: check if the process which has created the lock file is still running!
    return LockStatus::StaleLock;
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void DirectoryLock::lock() throw (Exception)
{
    // check if the directory to lock does exist
    if (!mDirToLock.isExistingDir()) {
        throw RuntimeError(__FILE__, __LINE__, QString(),  QString(
            tr("The directory \"%1\" does not exist.")).arg(mDirToLock.toNative()));
    }

    // when the directory is valid, the lock filepath must be valid too
    Q_ASSERT(mLockFilePath.isValid());

    // prepare the content which will be written to the lock file
    QStringList lines;
    lines.append(SystemInfo::getFullUsername().remove("\n"));
    lines.append(SystemInfo::getUsername().remove("\n"));
    lines.append(SystemInfo::getHostname().remove("\n"));
    lines.append(QString::number(QCoreApplication::applicationPid()));
    lines.append(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    QByteArray utf8content = lines.join('\n').toUtf8();

    // create/overwrite lock file
    FileUtils::writeFile(mLockFilePath, utf8content); // can throw

    // File Lock successfully created
    mLockedByThisObject = true;
}

void DirectoryLock::unlock() throw (Exception)
{
    // remove the lock file
    FileUtils::removeFile(mLockFilePath); // can throw

    // File Lock successfully removed
    mLockedByThisObject = false;
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
