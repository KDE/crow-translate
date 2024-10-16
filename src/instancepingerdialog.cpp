#include "instancepingerdialog.h"

#include "cmake.h"
#include "instancepinger.h"

#include <QDebug>
#include <QEventLoop>

InstancePingerDialog::InstancePingerDialog(QWidget *parent)
{
    // Setting range to 0, 0 works as busy widget
    setRange(0, 0);
    setLabelText(tr("Detecting fastes instance..."));
    setWindowTitle(APPLICATION_NAME);

    InstancePinger *pinger = new InstancePinger(this);

    connect(pinger, &InstancePinger::finished, [this](const QString &url) {
        m_url = url;
        accept();
    });
}

QString InstancePingerDialog::getUrl() const
{
    if (m_url.isEmpty()) {
        return InstancePinger::defaultInstance();
    }

    return m_url;
}
