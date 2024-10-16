#ifndef INSTANCEPINGERDIALOG_H
#define INSTANCEPINGERDIALOG_H

#include "instancepinger.h"

#include <QProgressDialog>

class InstancePingerDialog : public QProgressDialog
{
    Q_OBJECT

public:
    InstancePingerDialog(QWidget *parent = nullptr);

    void start();

signals:
    void finished(const QString &url);
    void canceled(const QString &url);
};

#endif // INSTANCEPINGERDIALOG_H
