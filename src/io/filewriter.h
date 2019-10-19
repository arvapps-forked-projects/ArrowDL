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

#ifndef IO_FILE_WRITER_H
#define IO_FILE_WRITER_H

#include <Io/IFileHandler>

#include <QtCore/QCoreApplication>

class QIODevice;

class FileWriter
{
    Q_DECLARE_TR_FUNCTIONS(FileWriter)

public:
    enum FileWriterError {
        UnknownError,
        DeviceError,
        UnsupportedFormatError,
        InvalidFileError
    };

    FileWriter();
    explicit FileWriter(QIODevice *device);
    explicit FileWriter(const QString &fileName);
    ~FileWriter();


    bool canWrite();
    bool write(DownloadEngine *engine);

    FileWriterError error() const;
    QString errorString() const;

    static QString supportedFormats();

private:
    /* Device */
    QIODevice *m_device;
    IFileHandler *m_handler;

    bool canWriteHelper();
    IFileHandler *createWriteHandlerHelper(QIODevice *device);

    /* Error */
    FileWriter::FileWriterError m_fileWriterError;
    QString m_errorString;    

    Q_DISABLE_COPY(FileWriter)
};

#endif // IO_FILE_WRITER_H