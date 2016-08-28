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

#ifndef LIBREPCB_WORKSPACE_LIBRARYINFOWIDGET_H
#define LIBREPCB_WORKSPACE_LIBRARYINFOWIDGET_H

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include <QtWidgets>
#include <librepcbcommon/exceptions.h>

/*****************************************************************************************
 *  Namespace / Forward Declarations
 ****************************************************************************************/
namespace librepcb {

namespace library {
class Library;
}

namespace workspace {

class Workspace;

namespace Ui {
class LibraryInfoWidget;
}

/*****************************************************************************************
 *  Class LibraryInfoWidget
 ****************************************************************************************/

/**
 * @brief The LibraryInfoWidget class
 *
 * @author ubruhin
 * @date 2016-08-03
 */
class LibraryInfoWidget final : public QWidget
{
        Q_OBJECT

    public:

        // Constructors / Destructor
        LibraryInfoWidget() noexcept;
        LibraryInfoWidget(const LibraryInfoWidget& other) = delete;
        LibraryInfoWidget(const Workspace& ws, const library::Library& lib) noexcept;
        ~LibraryInfoWidget() noexcept;

        // Getters
        QString getName() const noexcept;

        // Operator Overloadings
        LibraryInfoWidget& operator=(const LibraryInfoWidget& rhs) = delete;


    private: // Data

        QScopedPointer<Ui::LibraryInfoWidget> mUi;
};

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace workspace
} // namespace librepcb

#endif // LIBREPCB_WORKSPACE_LIBRARYINFOWIDGET_H
