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
#include <QFileDialog>
#include "workspace.h"
#include <librepcbcommon/exceptions.h>
#include <librepcbcommon/fileio/filepath.h>
#include <librepcbcommon/fileio/fileutils.h>
#include <librepcbcommon/application.h>
#include <librepcblibraryeditor/libraryeditor.h>
#include <librepcbproject/project.h>
#include "library/workspacelibrary.h"
#include "projecttreemodel.h"
#include "recentprojectsmodel.h"
#include "favoriteprojectsmodel.h"
#include "settings/workspacesettings.h"
#include <librepcbcommon/schematiclayer.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

using namespace library;
using namespace project;

namespace workspace {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

Workspace::Workspace(const FilePath& wsPath) throw (Exception) :
    QObject(nullptr),
    mPath(wsPath),
    mProjectsPath(mPath.getPathTo("projects")),
    mVersionPath(mPath.getPathTo("v" % qApp->getFileFormatVersion().toStr())),
    mMetadataPath(mVersionPath.getPathTo("metadata")),
    mLibrariesPath(mVersionPath.getPathTo("libraries")),
    mLock(mVersionPath)
{
    // check directory paths
    if (!isValidWorkspacePath(mPath)) {
        throw RuntimeError(__FILE__, __LINE__, mPath.toStr(),
            QString(tr("Invalid workspace path: \"%1\"")).arg(mPath.toNative()));
    }
    if ((!mProjectsPath.isValid()) || (!mVersionPath.isValid())  ||
        (!mMetadataPath.isValid()) || (!mLibrariesPath.isValid()))
    {
        throw LogicError(__FILE__, __LINE__, mPath.toStr(), QString());
    }

    // Check if the workspace is locked (already open or application was crashed).
    switch (mLock.getStatus()) // throws an exception on error
    {
        case FileLock::LockStatus_t::Unlocked:
            break; // nothing to do here (the workspace will be locked later)

        case FileLock::LockStatus_t::Locked:
        {
            // the workspace is locked by another application instance
            throw RuntimeError(__FILE__, __LINE__, QString(), tr("The workspace is already "
                               "opened by another application instance or user!"));
        }

        case FileLock::LockStatus_t::StaleLock:
        {
            // ignore stale lock as there is nothing to restore
            qWarning() << "There was a stale lock on the workspace:" << mPath;
            break;
        }

        default: Q_ASSERT(false); throw LogicError(__FILE__, __LINE__);
    }

    // the workspace can be opened by this application, so we will lock it
    mLock.lock(); // throws an exception on error

    // create directories (if not already exist)
    mProjectsPath.mkPath();
    mMetadataPath.mkPath();
    mLibrariesPath.mkPath();

    // all OK, let's load the workspace stuff!
    mWorkspaceSettings.reset(new WorkspaceSettings(*this));
    mLibrary.reset(new WorkspaceLibrary(*this));
    mRecentProjectsModel.reset(new RecentProjectsModel(*this));
    mFavoriteProjectsModel.reset(new FavoriteProjectsModel(*this));
    mProjectTreeModel.reset(new ProjectTreeModel(*this));
}

Workspace::~Workspace() noexcept
{
}

/*****************************************************************************************
 *  Getters
 ****************************************************************************************/

QAbstractItemModel& Workspace::getProjectTreeModel() const noexcept
{
    return *mProjectTreeModel;
}

QAbstractItemModel& Workspace::getRecentProjectsModel() const noexcept
{
    return *mRecentProjectsModel;
}

QAbstractItemModel& Workspace::getFavoriteProjectsModel() const noexcept
{
    return *mFavoriteProjectsModel;
}

/*****************************************************************************************
 *  Project Management
 ****************************************************************************************/

void Workspace::setLastRecentlyUsedProject(const FilePath& filepath) noexcept
{
    mRecentProjectsModel->setLastRecentProject(filepath);
}

bool Workspace::isFavoriteProject(const FilePath& filepath) const noexcept
{
    return mFavoriteProjectsModel->isFavoriteProject(filepath);
}

void Workspace::addFavoriteProject(const FilePath& filepath) noexcept
{
    mFavoriteProjectsModel->addFavoriteProject(filepath);
}

void Workspace::removeFavoriteProject(const FilePath& filepath) noexcept
{
    mFavoriteProjectsModel->removeFavoriteProject(filepath);
}

/*****************************************************************************************
 *  Static Methods
 ****************************************************************************************/

bool Workspace::isValidWorkspacePath(const FilePath& path) noexcept
{
    return path.getPathTo(".librepcb-workspace").isExistingFile();
}

QList<Version> Workspace::getFileFormatVersionsOfWorkspace(const FilePath& path) noexcept
{
    QList<Version> list;
    if (isValidWorkspacePath(path)) {
        QDir dir(path.toStr());
        QStringList subdirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        foreach (const QString& subdir, subdirs) {
            if (subdir.startsWith('v')) {
                Version version(subdir.mid(1, -1));
                if (version.isValid()) {
                    list.append(version);
                }
            }
        }
        qSort(list);
    }
    return list;
}

void Workspace::createNewWorkspace(const FilePath& path) throw (Exception)
{
    FileUtils::writeFile(path.getPathTo(".librepcb-workspace"), QByteArray()); // can throw
}

FilePath Workspace::getMostRecentlyUsedWorkspacePath() noexcept
{
    QSettings clientSettings;
    return FilePath(clientSettings.value("workspaces/most_recently_used").toString());
}

void Workspace::setMostRecentlyUsedWorkspacePath(const FilePath& path) noexcept
{
    QSettings clientSettings;
    clientSettings.setValue("workspaces/most_recently_used", path.toNative());
}

FilePath Workspace::chooseWorkspacePath() noexcept
{
    FilePath path(QFileDialog::getExistingDirectory(0, tr("Select Workspace Path")));

    if ((path.isValid()) && (!isValidWorkspacePath(path))) {
        int answer = QMessageBox::question(0, tr("Create new workspace?"),
                        tr("The specified workspace does not exist. "
                           "Do you want to create a new workspace?"));

        if (answer == QMessageBox::Yes) {
            try {
                createNewWorkspace(path); // can throw
            } catch (const Exception& e) {
                QMessageBox::critical(0, tr("Error"), tr("Could not create the workspace!"));
                return FilePath();
            }
        }
        else {
            return FilePath();
        }
    }

    return path;
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace workspace
} // namespace librepcb
