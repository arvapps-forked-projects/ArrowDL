/* - DownZemAll! - Copyright (C) 2019 Sebastien Vavassori
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

#include "wizarddialog.h"
#include "ui_wizarddialog.h"

#include <Core/HtmlParser>
#include <Core/DownloadItem>
#include <Core/DownloadManager>
#include <Core/Model>
#include <Core/ResourceItem>
#include <Core/ResourceModel>
#include <Core/Settings>
#include <Ipc/InterProcessCommunication>

#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QtMath>
#include <QtCore/QUrl>
#include <QtCore/QSettings>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMessageBox>

#ifdef USE_QT_WEBENGINE
#  include <QtWebEngineWidgets/QWebEngineView>
#  include <QtWebEngineWidgets/QWebEngineSettings>
#else
#  include <QtNetwork/QNetworkAccessManager>
#  include <QtNetwork/QNetworkRequest>
#  include <QtNetwork/QNetworkReply>
#endif


static QList<IDownloadItem*> createItems( QList<ResourceItem*> resources, DownloadManager *downloadManager)
{
    QList<IDownloadItem*> items;
    foreach (auto resource, resources) {
        DownloadItem* item = new DownloadItem(downloadManager);
        item->setResource(resource);
        items << item;
    }
    return items;
}


WizardDialog::WizardDialog(DownloadManager *downloadManager,
                           Settings *settings, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::WizardDialog)
    , m_downloadManager(downloadManager)
    , m_model(new Model(this))
    #ifdef USE_QT_WEBENGINE
    , m_webEngineView(Q_NULLPTR)
    #else
    , m_networkAccessManager(new QNetworkAccessManager(this))
    #endif
    , m_settings(settings)
{
    ui->setupUi(this);

    ui->pathWidget->setPathType(PathWidget::Directory);
    ui->linkWidget->setModel(m_model);

    connect(m_settings, SIGNAL(changed()), this, SLOT(refreshFilters()));

    connect(ui->pathWidget, SIGNAL(currentPathChanged(QString)), m_model, SLOT(setDestination(QString)));
    connect(ui->pathWidget, SIGNAL(currentPathChanged(QString)), this, SLOT(onChanged(QString)));

    connect(ui->maskWidget, SIGNAL(currentMaskChanged(QString)), m_model, SLOT(setMask(QString)));
    connect(ui->maskWidget, SIGNAL(currentMaskChanged(QString)), this, SLOT(onChanged(QString)));

    connect(ui->filterWidget, SIGNAL(regexChanged(QRegExp)), m_model, SLOT(select(QRegExp)));

    connect(m_model, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

    refreshFilters();

    readSettings();
}

WizardDialog::~WizardDialog()
{
    delete ui;
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}

void WizardDialog::accept()
{
    start(true);
    writeSettings();
    QDialog::accept();
}

void WizardDialog::acceptPaused()
{
    start(false);
    writeSettings();
    QDialog::accept();
}

void WizardDialog::reject()
{
    writeSettings();
    QDialog::reject();
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::start(bool started)
{
    if (m_downloadManager) {
        QList<IDownloadItem*> items = createItems(m_model->selection(), m_downloadManager);
        m_downloadManager->append(items, started);
    }
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::loadResources(const QString &message)
{
    parseResources(message);
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::loadUrl(const QUrl &url)
{
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("Error: The url is not valid:\n\n%0").arg(url.toString()));
    } else {
        m_url = url;

#ifdef USE_QT_WEBENGINE
        qDebug() << Q_FUNC_INFO << "GOOGLE GUMBO + QT WEB ENGINE";
        if (!m_webEngineView) {
            m_webEngineView = new QWebEngineView(this);

            connect(m_webEngineView, SIGNAL(loadProgress(int)), SLOT(onLoadProgress(int)));
            connect(m_webEngineView, SIGNAL(loadFinished(bool)), SLOT(onLoadFinished(bool)));

            /* Only load source, not media */
            QWebEngineSettings *settings =  m_webEngineView->settings()->globalSettings();
            settings->setAttribute(QWebEngineSettings::AutoLoadImages, false);
#if QT_VERSION >= 0x050700
            settings->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, false);
            m_webEngineView->page()->setAudioMuted(true);
#endif
#if QT_VERSION >= 0x051000
            settings->setAttribute(QWebEngineSettings::ShowScrollBars, false);
#endif
#if QT_VERSION >= 0x051300
            settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
#endif
        }
        m_webEngineView->load(m_url);
#else
        qDebug() << Q_FUNC_INFO << "GOOGLE GUMBO";
        QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(m_url));
        connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(onDownloadProgress(qint64,qint64)));
        connect(reply, SIGNAL(finished()), this, SLOT(onFinished()));
#endif
        setProgressInfo(0, tr("Connecting..."));
    }
}

/******************************************************************************
 ******************************************************************************/
#ifdef USE_QT_WEBENGINE
void WizardDialog::onLoadProgress(int progress)
{
    /* Between 1% and 90% */
    progress = qMin(qFloor(0.90 * progress), 90);
    setProgressInfo(progress, tr("Downloading..."));
}

void WizardDialog::onLoadFinished(bool finished)
{
    if (finished) {
        /*
         * Hack to retrieve the HTML page content from QWebEnginePage
         * and send it to the Gumbo HTML5 Parser.
         */
        connect(this, SIGNAL(htmlReceived(QString)), this, SLOT(onHtmlReceived(QString)));
        m_webEngineView->page()->toHtml([this](const QString &result) mutable
        {
            emit htmlReceived(result);
        });
        m_webEngineView->setVisible(false);

    } else {
        setNetworkError("");
    }
}

void WizardDialog::onHtmlReceived(QString content)
{
    QByteArray downloadedData = content.toUtf8();
    parseHtml(downloadedData);
}
#else
void WizardDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    /* Between 1% and 90% */
    int percent = 1;
    if (bytesTotal > 0) {
        percent = qMin(qFloor(90.0 * bytesReceived / bytesTotal), 90);
    }
    setProgressInfo(percent, tr("Downloading..."));
}

void WizardDialog::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply && reply->error() == QNetworkReply::NoError) {
        QByteArray downloadedData = reply->readAll();
        reply->deleteLater();
        parseHtml(downloadedData);
    } else {
        setNetworkError(reply->errorString());
    }
}
#endif

/******************************************************************************
 ******************************************************************************/
void WizardDialog::parseResources(QString message)
{
    setProgressInfo(10, tr("Collecting links..."));

    m_model->linkModel()->clear();
    m_model->contentModel()->clear();

    InterProcessCommunication::parseMessage(message, m_model);

    setProgressInfo(99, tr("Finished"));

    // Force update
    m_model->setDestination(ui->pathWidget->currentPath());
    m_model->setMask(ui->maskWidget->currentMask());
    m_model->select(ui->filterWidget->regex());

    onSelectionChanged();

    setProgressInfo(100);
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::parseHtml(const QByteArray &downloadedData)
{
    setProgressInfo(90, tr("Collecting links..."));

    m_model->linkModel()->clear();
    m_model->contentModel()->clear();

    qDebug() << m_url;
    qDebug() << "---------------------";
    qDebug() << downloadedData;
    qDebug() << "---------------------";

    HtmlParser htmlParser;
    htmlParser.parse(downloadedData, m_url, m_model);

    setProgressInfo(99, tr("Finished"));

    // Force update
    m_model->setDestination(ui->pathWidget->currentPath());
    m_model->setMask(ui->maskWidget->currentMask());
    m_model->select(ui->filterWidget->regex());

    onSelectionChanged();

    setProgressInfo(100);
}

void WizardDialog::setNetworkError(const QString &errorString)
{
    const QFontMetrics fontMetrics = this->fontMetrics();
    const QString elidedUrl =
            fontMetrics.elidedText(m_url.toString(), Qt::ElideRight,
                                   ui->progressPage->width() - 200);

    const QString message =
            tr("The wizard can't connect to URL:\n\n"
               "%0\n\n"
               "%1")
            .arg(elidedUrl)
            .arg(errorString);

    setProgressInfo(-1, message);
}

void WizardDialog::setProgressInfo(int percent, const QString &text)
{
    if (percent < 0) {
        ui->stackedWidget->setCurrentIndex(1);
        ui->progressBar->setValue(0);
        ui->progressBar->setVisible(false);
        ui->progressLabel->setText(text);

    } else if (percent >= 0 && percent < 100) {
        ui->stackedWidget->setCurrentIndex(1);
        ui->progressBar->setValue(percent);
        ui->progressBar->setVisible(true);
        ui->progressLabel->setText(text);

    } else { // percent >= 100
        ui->stackedWidget->setCurrentIndex(0);
    }
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::onSelectionChanged()
{
    const ResourceModel *currentModel = m_model->currentModel();
    const int selectionCount = currentModel->selectedResourceItems().count();
    if (selectionCount == 0) {
        ui->tipLabel->setText(tr("After selecting links, click on Start!"));
    } else {
        const int count = currentModel->resourceItems().count();
        ui->tipLabel->setText(tr("Selected links: %0 of %1").arg(selectionCount).arg(count));
    }
    onChanged(QString());
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::onChanged(QString)
{
    const ResourceModel *currentModel = m_model->currentModel();
    const int selectionCount = currentModel->selectedResourceItems().count();
    const bool enabled =
            !ui->pathWidget->currentPath().isEmpty() &&
            !ui->maskWidget->currentMask().isEmpty() &&
            selectionCount > 0;
    ui->startButton->setEnabled(enabled);
    ui->addPausedButton->setEnabled(enabled);
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::refreshFilters()
{
    QList<Filter> filters = m_settings->filters();
    ui->filterWidget->clearFilters();
    foreach (auto filter, filters) {
        ui->filterWidget->addFilter(filter.title, filter.regexp);
    }
}

/******************************************************************************
 ******************************************************************************/
void WizardDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup("Wizard");
    resize(settings.value("DialogSize", QSize(800, 600)).toSize());
    ui->filterWidget->setState(settings.value("FilterState", 0).toUInt());
    ui->filterWidget->setCurrentFilter(settings.value("Filter", QString()).toString());
    ui->filterWidget->setFilterHistory(settings.value("FilterHistory", QString()).toStringList());
    ui->linkWidget->setColumnWidths(settings.value("ColumnWidths").value<QList<int> >());
    ui->pathWidget->setCurrentPath(settings.value("Path", QString()).toString());
    ui->pathWidget->setPathHistory(settings.value("PathHistory").toStringList());
    ui->maskWidget->setCurrentMask(settings.value("Mask", QString()).toString());
    settings.endGroup();
}

void WizardDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup("Wizard");
    settings.setValue("DialogSize", size());
    settings.setValue("FilterState", ui->filterWidget->state());
    settings.setValue("Filter", ui->filterWidget->currentFilter());
    settings.setValue("FilterHistory", ui->filterWidget->filterHistory());
    settings.setValue("ColumnWidths", QVariant::fromValue(ui->linkWidget->columnWidths()));
    settings.setValue("Path", ui->pathWidget->currentPath());
    settings.setValue("PathHistory", ui->pathWidget->pathHistory());
    settings.setValue("Mask", ui->maskWidget->currentMask());
    settings.endGroup();
}
