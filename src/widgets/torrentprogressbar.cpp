/* - DownZemAll! - Copyright (C) 2019-present Sebastien Vavassori
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include "torrentprogressbar.h"

#include <Widgets/CustomStyleOptionProgressBar>

#include <QtCore/QDebug>
#include <QtGui/QColor>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QApplication>

/* Constant */
/// \todo these colors are from downloadqueueview.cpp
static const QColor s_black         = QColor(0, 0, 0);
static const QColor s_lightBlue     = QColor(205, 232, 255);
static const QColor s_darkGrey      = QColor(160, 160, 160);
static const QColor s_green         = QColor(170, 224, 97);
static const QColor s_darkGreen     = QColor(0, 143, 0);


/*!
 * \class TorrentProgressBar
 *
 * A 'segmented' QProgressBar.
 *
 */
TorrentProgressBar::TorrentProgressBar(QWidget *parent) : QProgressBar(parent)
  , m_downloadedPieces(QBitArray())
{    
    setRange(0, 100);
    setValue(0);
}

/******************************************************************************
 ******************************************************************************/
void TorrentProgressBar::clearPieces()
{
    m_downloadedPieces.clear();
    repaint();
}

void TorrentProgressBar::setPieces(const QBitArray &downloadedPieces)
{
    m_downloadedPieces = downloadedPieces;
    repaint();
}

/******************************************************************************
 ******************************************************************************/
void TorrentProgressBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    CustomStyleOptionProgressBar myOption;
    initStyleOption(&myOption);

    myOption.palette.setColor(QPalette::All, QPalette::Highlight, s_lightBlue);
    myOption.palette.setColor(QPalette::All, QPalette::HighlightedText, s_black);

    const int progress = value();

    CustomStyleOptionProgressBar progressBarOption;
    progressBarOption.state = QStyle::State_Enabled;
    progressBarOption.direction = QApplication::layoutDirection();
    progressBarOption.rect = myOption.rect;
    progressBarOption.fontMetrics = QApplication::fontMetrics();
    progressBarOption.minimum = 0;
    progressBarOption.maximum = 100;
    progressBarOption.textAlignment = Qt::AlignCenter;
    progressBarOption.textVisible = false;
    progressBarOption.palette.setColor(QPalette::All, QPalette::Highlight, s_lightBlue);
    progressBarOption.palette.setColor(QPalette::All, QPalette::HighlightedText, s_black);
    progressBarOption.progress = progress;
    progressBarOption.color = progress < 100 ? s_green : s_darkGreen;
    progressBarOption.icon = QIcon();

    progressBarOption.hasSegments = true;
    progressBarOption.segments = m_downloadedPieces;

    QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, &painter);
}