/*
 * EDA4U - Professional EDA for everyone!
 * Copyright (C) 2013 Urban Bruhin
 * http://eda4u.ubruhin.ch/
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
#include "schematicclipboard.h"
#include "../../common/file_io/xmldomelement.h"
#include "symbolinstance.h"

namespace project {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

SchematicClipboard::SchematicClipboard() noexcept :
    QObject(nullptr), mCutActive(false)
{
}

SchematicClipboard::~SchematicClipboard() noexcept
{
    clear();
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void SchematicClipboard::clear() noexcept
{
    qDeleteAll(mSymbolInstances);       mSymbolInstances.clear();
}

void SchematicClipboard::cut(const QList<SymbolInstance*>& symbols) throw (Exception)
{
    mCutActive = true;
    setElements(symbols);
}

void SchematicClipboard::copy(const QList<SymbolInstance*>& symbols) throw (Exception)
{
    mCutActive = false;
    setElements(symbols);
}

void SchematicClipboard::paste(Schematic& schematic, QList<SymbolInstance*>& symbols) throw (Exception)
{
    Q_ASSERT(symbols.isEmpty() == true);

    foreach (XmlDomElement* element, mSymbolInstances)
    {
        symbols.append(new SymbolInstance(schematic, *element));
    }

    mCutActive = false;
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void SchematicClipboard::setElements(const QList<SymbolInstance*>& symbols) throw (Exception)
{
    clear();

    foreach (SymbolInstance* symbol, symbols)
        mSymbolInstances.append(symbol->serializeToXmlDomElement());
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project