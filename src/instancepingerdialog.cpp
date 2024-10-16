#include "instancepingerdialog.h"

#include "cmake.h"

#include <QEventLoop>

InstancePingerDialog::InstancePingerDialog(QWidget *parent) {
    // Setting range to 0, 0 works as busy widget
    setRange(0, 0);
    setLabelText(tr("Detecting fastes instance..."));
    setWindowTitle(APPLICATION_NAME);
    setModal(true);
    show();
}

void InstancePingerDialog::start() {
    QEventLoop loop;
    InstancePinger pinger(this);
    connect(this, &InstancePingerDialog::canceled, &loop, [&]() {
        emit canceled(InstancePinger::defaultInstance());
        loop.quit();
    });
    connect(&pinger, &InstancePinger::finished, &loop, &QEventLoop::quit);
    connect(&pinger, &InstancePinger::finished, [&](const QString &url) {
        emit finished(url);
        loop.quit();
    });
    loop.exec();
}
