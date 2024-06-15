/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UPDATERDIALOG_H
#define UPDATERDIALOG_H

#include <QDialog>
#include <QUrl>

class QGitTag;
class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace Ui
{
class UpdaterDialog;
}

class UpdaterDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(UpdaterDialog)

public:
    explicit UpdaterDialog(QGitTag *release, int installer, QWidget *parent = nullptr);
    ~UpdaterDialog();

private slots:
    void download();
    void cancelDownload();
    void install();

    void showDownloadStatus();
    void saveDownloadedData();

private:
    void setStatus(const QString &errorString, bool success);

    Ui::UpdaterDialog *ui;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply;
    QFile *m_installerFile;

    QUrl m_downloadUrl;
    QString m_downloadPath;
};

#endif // UPDATERDIALOG_H
