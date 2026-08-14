/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Gotchas #6 and #7 from review-localai-backend.md. Both fixes (cb74d186)
// only exist on localai-backend-salvage, not this branch - the affected
// assertions are expected to start RED here and flip green once this test
// file lands on top of the salvage merge.

#include "mainwindow.h"
#include "mockhttpserver.h"
#include "singleapplication.h"
#include "testisolation.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "translator/atranslationprovider.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>

class SettingsDialogLocalAiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty())
            QSKIP("No display server available - skipping GUI tests");
    }

    // Gotcha #6: "Refresh models" blocks the whole dialog on a nested
    // QEventLoop for up to the network timeout, and stays clickable while a
    // refresh is already in flight - a second click before the first
    // response arrives fires a second concurrent request. Reproducing the
    // re-entrant-click race itself proved unreliable to do synchronously in
    // this unit test (scheduling a callback to fire *during* the nested
    // QEventLoop hung rather than racing cleanly, likely an interaction
    // between the nested loop, QTest's synthetic event delivery, and this
    // sandboxed environment's event dispatcher) - that half of the
    // regression moves to crow-flake/e2e/, which can drive a real click on
    // a real running app instead of fighting a synthetic one. This test
    // keeps the functional smoke coverage: a refresh against a real
    // (mocked) endpoint actually populates the model list.
    void testRefreshModelsPopulatesModelList()
    {
        MockHttpServer server;
        server.queueJson(200, QByteArrayLiteral(R"({"data":[{"id":"model-a"},{"id":"model-b"}]})"));

        AppSettings settings;
        settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Copy);

        MainWindow window;
        SettingsDialog dialog(&window);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));

        // LocalAI is one page among several in the settings sidebar
        // (General/Interface/.../LocalAI/OCR/...) - the dialog defaults to
        // showing a different page, and a page that isn't current in its
        // QStackedWidget never gets real geometry (its children end up 0px
        // tall and !isVisible(), so QTest::mouseClick lands on nothing).
        auto *localAiPage = dialog.findChild<QWidget *>(QStringLiteral("localAiPage"));
        QVERIFY(localAiPage != nullptr);
        auto *pagesStack = dialog.findChild<QStackedWidget *>(QStringLiteral("pagesStackedWidget"));
        QVERIFY(pagesStack != nullptr);
        // SettingsDialog::setCurrentPage() is private; the .ui file wires
        // pagesListWidget's currentRowChanged(int) to it via a designer
        // connection (list row index == stack page index), so driving the
        // public list widget is the real, supported way to switch pages.
        auto *pagesList = dialog.findChild<QListWidget *>(QStringLiteral("pagesListWidget"));
        QVERIFY(pagesList != nullptr);
        pagesList->setCurrentRow(pagesStack->indexOf(localAiPage));

        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("localAiTabWidget"));
        QVERIFY(tabs != nullptr);
        tabs->setCurrentIndex(0);
        QWidget *ollamaPage = tabs->widget(0);
        QVERIFY(ollamaPage != nullptr);

        auto *urlEdit = ollamaPage->findChild<QLineEdit *>(QStringLiteral("localAiUrlEdit"));
        QVERIFY(urlEdit != nullptr);
        urlEdit->setText(server.baseUrl());

        // The tab now has separate Text/Vision model combos (vision-mode UI);
        // the dialog defaults to the Text sub-page, which is what "Refresh
        // models" against a plain chat-completions mock exercises.
        auto *modelCombo = ollamaPage->findChild<QComboBox *>(QStringLiteral("localAiTextModelCombo"));
        QVERIFY(modelCombo != nullptr);

        QPushButton *refreshButton = ollamaPage->findChild<QPushButton *>(QStringLiteral("localAiRefreshButton"));
        QVERIFY(refreshButton != nullptr);
        QVERIFY(QTest::qWaitFor([refreshButton]() {
            return refreshButton->isVisible() && refreshButton->height() > 0;
        },
                                2000));

        QTest::mouseClick(refreshButton, Qt::LeftButton);

        // QNetworkAccessManager::get() is async - the request only reaches
        // the mock server after the event loop spins for connect+write.
        QVERIFY(QTest::qWaitFor([&server]() {
            return server.requestCount() == 1;
        },
                                5000));
        QCOMPARE(server.requestCount(), 1);
        QVERIFY(server.requestPath(0).contains(QStringLiteral("/v1/models")));
        QVERIFY(QTest::qWaitFor([modelCombo]() {
            return modelCombo->count() == 2;
        },
                                3000));
        QCOMPARE(modelCombo->itemText(0), QStringLiteral("model-a"));
        QCOMPARE(modelCombo->itemText(1), QStringLiteral("model-b"));
    }

    // Gotcha #7: switching Copy -> LocalAI -> Mozhi must restore the
    // original Mozhi engine list. The restore heuristic
    // (itemText(0) != m_engineItemsText.at(0)) is redundant with the
    // capability branch it's already inside; fixed by dropping it and
    // restoring unconditionally.
    void testEngineComboRestoredAfterLocalAiRoundTrip()
    {
        AppSettings settings;
        settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Mozhi);

        MainWindow window;
        auto *engineCombo = window.findChild<QComboBox *>(QStringLiteral("engineComboBox"));
        QVERIFY(engineCombo != nullptr);
        QVERIFY2(engineCombo->count() > 0, "Mozhi engine combo should be populated at startup");

        const QStringList originalItems = [&] {
            QStringList items;
            for (int i = 0; i < engineCombo->count(); ++i)
                items << engineCombo->itemText(i);
            return items;
        }();

        settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::LocalAI);
        window.close();
        MainWindow window2;
        auto *engineCombo2 = window2.findChild<QComboBox *>(QStringLiteral("engineComboBox"));
        QVERIFY(engineCombo2 != nullptr);

        settings.setTranslationProviderBackend(ATranslationProvider::ProviderBackend::Mozhi);
        window2.close();
        MainWindow window3;
        auto *engineCombo3 = window3.findChild<QComboBox *>(QStringLiteral("engineComboBox"));
        QVERIFY(engineCombo3 != nullptr);

        QStringList restoredItems;
        for (int i = 0; i < engineCombo3->count(); ++i)
            restoredItems << engineCombo3->itemText(i);

        QCOMPARE(restoredItems, originalItems);
    }
};

int main(int argc, char *argv[])
{
    isolateTestSettings();

    SingleApplication app(argc, argv, true);
    SettingsDialogLocalAiTest tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_settingsdialog_localai.moc"
