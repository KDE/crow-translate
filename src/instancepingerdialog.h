#ifndef INSTANCEPINGERDIALOG_H
#define INSTANCEPINGERDIALOG_H

#include <QProgressDialog>

class InstancePingerDialog : public QProgressDialog
{
    Q_OBJECT

public:
    InstancePingerDialog(QWidget *parent = nullptr);

    QString getUrl() const;

private:
    QString m_url;
};

#endif // INSTANCEPINGERDIALOG_H
