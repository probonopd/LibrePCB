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
#include "gerbergenerator.h"
#include "gerberaperturelist.h"
#include "../geometry/ellipse.h"
#include "../geometry/polygon.h"
#include "../fileio/smarttextfile.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

GerberGenerator::GerberGenerator(const QString& projName, const Uuid& projUuid,
                                 const QString& projRevision) noexcept :
    mProjectId(QString(projName).remove(QChar(','))),
    mProjectGuid(projUuid.toStr().remove(QChar('-'))),
    mProjectRevision(projRevision), mOutput(), mContent(),
    mApertureList(new GerberApertureList()), mCurrentApertureNumber(-1),
    mMultiQuadrantArcModeOn(false)
{
}

GerberGenerator::~GerberGenerator() noexcept
{
}

/*****************************************************************************************
 *  Plot Methods
 ****************************************************************************************/

void GerberGenerator::setLayerPolarity(LayerPolarity p) noexcept
{
    switch (p)
    {
        case LayerPolarity::Positive: mContent.append("%LPD*%\n"); break;
        case LayerPolarity::Negative: mContent.append("%LPC*%\n"); break;
        default: qCritical() << "Invalid Layer Polarity:" << static_cast<int>(p); break;
    }
}

void GerberGenerator::drawLine(const Point& start, const Point& end, const Length& width) noexcept
{
    setCurrentAperture(mApertureList->setCircle(width, Length(0)));
    moveToPosition(start);
    linearInterpolateToPosition(end);
}

void GerberGenerator::drawEllipseOutline(const Ellipse& ellipse) noexcept
{
    if (ellipse.getRadiusX() == ellipse.getRadiusY()) {
        Length outerDia = (ellipse.getRadiusX() * 2) + ellipse.getLineWidth();
        Length innerDia = (ellipse.getRadiusX() * 2) - ellipse.getLineWidth();
        if (innerDia < 0) innerDia = 0;
        flashCircle(ellipse.getCenter(), outerDia, innerDia);
    } else {
        // TODO!
        qWarning() << "Ellipse was ignored in gerber output!";
    }
}

void GerberGenerator::drawEllipseArea(const Ellipse& ellipse) noexcept
{
    if (ellipse.getRadiusX() == ellipse.getRadiusY()) {
        flashCircle(ellipse.getCenter(), ellipse.getRadiusX() * 2, Length(0));
    } else {
        // TODO!
        qWarning() << "Ellipse was ignored in gerber output!";
    }
}

void GerberGenerator::drawPolygonOutline(const Polygon& polygon) noexcept
{
    setCurrentAperture(mApertureList->setCircle(polygon.getLineWidth(), Length(0)));
    moveToPosition(polygon.getStartPos());
    for (int i = 0; i < polygon.getSegmentCount(); ++i) {
        const PolygonSegment* segment = polygon.getSegment(i); Q_ASSERT(segment);
        if (segment->getAngle() == 0) {
            // linear segment
            linearInterpolateToPosition(segment->getEndPos());
        } else {
            // arc segment
            if (segment->getAngle().abs() <= Angle::deg90()) {
                setMultiQuadrantArcModeOff();
            } else {
                setMultiQuadrantArcModeOn();
            }
            if (segment->getAngle() < 0) {
                switchToCircularCwInterpolationModeG02();
            } else {
                switchToCircularCcwInterpolationModeG03();
            }
            circularInterpolateToPosition(polygon.getStartPointOfSegment(i),
                                          polygon.calcCenterOfArcSegment(i),
                                          segment->getEndPos());
            switchToLinearInterpolationModeG01();
        }
    }
}

void GerberGenerator::drawPolygonArea(const Polygon& polygon) noexcept
{
    setCurrentAperture(mApertureList->setCircle(Length(0), Length(0)));
    setRegionModeOn();
    moveToPosition(polygon.getStartPos());
    for (int i = 0; i < polygon.getSegmentCount(); ++i) {
        const PolygonSegment* segment = polygon.getSegment(i); Q_ASSERT(segment);
        if (segment->getAngle() == 0) {
            // linear segment
            linearInterpolateToPosition(segment->getEndPos());
        } else {
            // arc segment
            if (segment->getAngle().abs() <= Angle::deg90()) {
                setMultiQuadrantArcModeOff();
            } else {
                setMultiQuadrantArcModeOn();
            }
            if (segment->getAngle() < 0) {
                switchToCircularCwInterpolationModeG02();
            } else {
                switchToCircularCcwInterpolationModeG03();
            }
            circularInterpolateToPosition(polygon.getStartPointOfSegment(i),
                                          polygon.calcCenterOfArcSegment(i),
                                          segment->getEndPos());
            switchToLinearInterpolationModeG01();
        }
    }
    if (!polygon.isClosed()) {
        qCritical() << "Accidentally generated gerber export of a non-closed polygon!";
        linearInterpolateToPosition(polygon.getStartPos());
    }
    setRegionModeOff();
}

void GerberGenerator::flashCircle(const Point& pos, const Length& dia, const Length& hole) noexcept
{
    setCurrentAperture(mApertureList->setCircle(dia, hole));
    flashAtPosition(pos);
}

void GerberGenerator::flashRect(const Point& pos, const Length& w, const Length& h,
                                const Angle& rot, const Length& hole) noexcept
{
    setCurrentAperture(mApertureList->setRect(w, h, rot, hole));
    flashAtPosition(pos);
}

void GerberGenerator::flashObround(const Point& pos, const Length& w, const Length& h,
                                   const Angle& rot, const Length& hole) noexcept
{
    setCurrentAperture(mApertureList->setObround(w, h, rot, hole));
    flashAtPosition(pos);
}

void GerberGenerator::flashRegularPolygon(const Point& pos, const Length& dia, int n,
                                          const Angle& rot, const Length& hole) noexcept
{
    setCurrentAperture(mApertureList->setRegularPolygon(dia, n, rot, hole));
    flashAtPosition(pos);
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void GerberGenerator::reset() noexcept
{
    mOutput.clear();
    mContent.clear();
    mApertureList->reset();
    mCurrentApertureNumber = -1;
}

void GerberGenerator::generate() throw (Exception)
{
    mOutput.clear();
    printHeader();
    printApertureList();
    printContent();
    printFooter();
}

void GerberGenerator::saveToFile(const FilePath& filepath) const throw (Exception)
{
    QScopedPointer<SmartTextFile> file(SmartTextFile::create(filepath));
    file->setContent(mOutput.toLatin1());
    file->save(true);
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void GerberGenerator::setCurrentAperture(int number) noexcept
{
    if (number != mCurrentApertureNumber) {
        mContent.append(QString("D%1*\n").arg(number));
        mCurrentApertureNumber = number;
    }
}

void GerberGenerator::setRegionModeOn() noexcept
{
    mContent.append("G36*\n");
}

void GerberGenerator::setRegionModeOff() noexcept
{
    mContent.append("G37*\n");
}

void GerberGenerator::setMultiQuadrantArcModeOn() noexcept
{
    if (!mMultiQuadrantArcModeOn) {
        mContent.append("G75*\n");
        mMultiQuadrantArcModeOn = true;
    }
}

void GerberGenerator::setMultiQuadrantArcModeOff() noexcept
{
    if (mMultiQuadrantArcModeOn) {
        mContent.append("G74*\n");
        mMultiQuadrantArcModeOn = false;
    }
}

void GerberGenerator::switchToLinearInterpolationModeG01() noexcept
{
    mContent.append("G01*\n");
}

void GerberGenerator::switchToCircularCwInterpolationModeG02() noexcept
{
    mContent.append("G02*\n");
}

void GerberGenerator::switchToCircularCcwInterpolationModeG03() noexcept
{
    mContent.append("G03*\n");
}

void GerberGenerator::moveToPosition(const Point& pos) noexcept
{
    mContent.append(QString("X%1Y%2D02*\n").arg(pos.getX().toNmString(),
                                                pos.getY().toNmString()));
}

void GerberGenerator::linearInterpolateToPosition(const Point& pos) noexcept
{
    mContent.append(QString("X%1Y%2D01*\n").arg(pos.getX().toNmString(),
                                                pos.getY().toNmString()));
}

void GerberGenerator::circularInterpolateToPosition(const Point& start, const Point& center, const Point& end) noexcept
{
    Point diff = center - start;
    if (!mMultiQuadrantArcModeOn) {
        diff.makeAbs(); // no sign allowed in single quadrant mode!
    }
    mContent.append(QString("X%1Y%2I%3J%4D01*\n").arg(end.getX().toNmString(),
                                                      end.getY().toNmString(),
                                                      diff.getX().toNmString(),
                                                      diff.getY().toNmString()));
}

void GerberGenerator::flashAtPosition(const Point& pos) noexcept
{
    mContent.append(QString("X%1Y%2D03*\n").arg(pos.getX().toNmString(),
                                                pos.getY().toNmString()));
}

void GerberGenerator::printHeader() noexcept
{
    mOutput.append("G04 --- HEADER BEGIN --- *\n");

    // add some X2 attributes
    mOutput.append(QString("%TF.GenerationSoftware,LibrePCB,LibrePCB,%1*%\n").arg(qApp->applicationVersion()));
    mOutput.append(QString("%TF.CreationDate,%1*%\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    mOutput.append(QString("%TF.ProjectId,%1,%2,%3*%\n").arg(mProjectId, mProjectGuid, mProjectRevision));
    mOutput.append("%TF.Part,Single*%\n"); // "Single" means "this is a PCB"
    //mOutput.append("%TF.FilePolarity,Positive*%\n");

    // coordinate format specification:
    //  - leading zeros omitted
    //  - absolute coordinates
    //  - coordiante format "6.6" --> allows us to directly use LengthBase_t (nanometers)!
    mOutput.append("%FSLAX66Y66*%\n");

    // set unit to millimeters
    mOutput.append("%MOMM*%\n");

    // start linear interpolation mode
    mOutput.append("G01*\n");

    // use single quadrant arc mode
    mOutput.append("G74*\n");

    mOutput.append("G04 --- HEADER END --- *\n");
}

void GerberGenerator::printApertureList() noexcept
{
    mOutput.append(mApertureList->generateString());
}

void GerberGenerator::printContent() noexcept
{
    mOutput.append("G04 --- BOARD BEGIN --- *\n");
    mOutput.append(mContent);
    mOutput.append("G04 --- BOARD END --- *\n");
}

void GerberGenerator::printFooter() noexcept
{
    // MD5 checksum over content
    mOutput.append(QString("%TF.MD5,%1*%\n").arg(calcOutputMd5Checksum()));

    // end of file
    mOutput.append("M02*\n");
}

QString GerberGenerator::calcOutputMd5Checksum() const noexcept
{
    // according to the RS-274C standard, linebreaks are not included in the checksum
    QString data = QString(mOutput).remove(QChar('\n'));
    return QString(QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5).toHex());
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
