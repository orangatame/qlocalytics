#include <QtTest/QtTest>
#include <QLocalytics/QLocalyticsDatabase>
#include <QLocalytics/QLocalyticsSession>
#include <QLocalytics/QLocalyticsUploader>

class SessionTest : public QObject
{
    Q_OBJECT
    
    
private slots:
  void testCase1();
  void testEscapeStrings();
  void testEscapeStrings_data();
};


void SessionTest::testCase1()
{
  LocalyticsSession *session = LocalyticsSession::sharedLocalyticsSession();
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QVERIFY(db);
  QVERIFY(session);
  db->resetAnalyticsData();
  db->vacuumIfRequired();

  QString appkey(QLatin1String("b8ebdecee388a9cb1219c89-1bb6b05a-2af6-11e2-6265-00ef75f32667"));
  session->init(appkey);

  QCOMPARE(db->eventCount(), 0);
  session->setOptIn(false);
  QCOMPARE(session->isOptedIn(), false);
  QCOMPARE(db->eventCount(), 1);

  session->setOptIn(true);
  QVERIFY(session->isOptedIn() == true);
  QVERIFY(db->eventCount() == 2);

  session->open(); // eventCount()++
  QDateTime sessionStarted = db->lastSessionStartTimestamp();
  QVERIFY(db->eventCount() == 3);

  QVERIFY(!sessionStarted.isNull());
  QVERIFY(sessionStarted.secsTo(QDateTime::currentDateTime()) <= 2);

  session->close();
  QVERIFY(db->eventCount() == 3);

  // resume() returns true if the session was resumed, and returns
  // `false` if the session was closed and a new session was opened.
  QVERIFY(session->resume());
  QVERIFY(db->eventCount() == 3);
  
  QSignalSpy spy(LocalyticsUploader::sharedLocalyticsUploader(), SIGNAL(uploadComplete()));
  session->upload();
  while (spy.count() == 0) {
    qApp->processEvents();
    QTest::qWait(200);
  }

}


void SessionTest::testEscapeStrings_data()
{
  // Test data inspired from 
  // http://deron.meranda.us/python/comparing_json_modules/strings
  QTest::addColumn<QString>("string");
  QTest::addColumn<QString>("result");
  
  QTest::newRow("no escaping") << "foo" << "foo";
  QTest::newRow("bell")        << "\x08" << "\\b";
  QTest::newRow("tab")         << "\x09" << "\\t";
  QTest::newRow("newline")     << "\x0a" << "\\n";
  QTest::newRow("form feed")   << "\x0c" << "\\f";
  QTest::newRow("carriage return") << "\x0d" << "\\r";
  QTest::newRow("single quote") << "Hawai'i" << "Hawai\\'i";
}

void SessionTest::testEscapeStrings()
{
  QFETCH(QString, string);
  QFETCH(QString, result);
  LocalyticsSession *session = LocalyticsSession::sharedLocalyticsSession();
  QCOMPARE(session->escapeString(string), result);
}

QTEST_MAIN(SessionTest)
#ifdef QMAKE_BUILD
#include "testsession.moc"
#else
#include "moc_testsession.cxx"
#endif
