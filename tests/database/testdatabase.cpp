#include <QtTest/QtTest>
#include <QLocalytics/QLocalyticsDatabase>

class DatabaseTest : public QObject
{
    Q_OBJECT
    
    
private slots:
  void testCase1();
  void testGettersAndSetters();
  void testEvents();
  void testTransactions();
  void testCustomDimensions();
};


void DatabaseTest::testCase1()
{
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QVERIFY(db);
  QVERIFY(db->eventCount() == 0);
  QDateTime createdTimestamp = db->createdTimestamp();
  QVERIFY(!createdTimestamp.isNull());
  QVERIFY(createdTimestamp.isValid());
  QVERIFY(createdTimestamp.secsTo(QDateTime::currentDateTime()) <= 2);
  QVERIFY(db->schemaVersion() == 7);

  QVERIFY(db->eventCount() == 0);
}

void DatabaseTest::testGettersAndSetters()
{
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();

  QVERIFY(db->appKey() == QLatin1String(""));

  db->setCustomerId(QLatin1String("foobarCustomerId"));
  QVERIFY(db->customerId() == QLatin1String("foobarCustomerId"));

  db->updateAppKey(QLatin1String("foobarAppKey"));
  QVERIFY(db->appKey() == QLatin1String("foobarAppKey"));

  db->setOptedOut(false);
  QVERIFY(!db->isOptedOut());
  db->setOptedOut(true);
  QVERIFY(db->isOptedOut());
}
void DatabaseTest::testEvents()
{
    LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
    //clear it out:
    db->resetAnalyticsData();
    int a, b = 0;
    bool success;
    db->incrementLastUploadNumber(&a);
    QVERIFY(a == 1);
    db->incrementLastUploadNumber(&a);
    QVERIFY(a == 2);

    db->incrementLastSessionNumber(&b);
    QVERIFY(b == 1);
    db->incrementLastSessionNumber(&b);
    QVERIFY(b == 2);

    QVERIFY(db->eventCount() == 0);

    db->addEventWithBlobString(QLatin1String("{blobEvent}"));
    QVERIFY(db->eventCount() == 1);

    db->addFlowEventWithBlobString(QLatin1String("{flowEvent}"));
    QVERIFY(db->eventCount() == 2);
    success = db->removeLastCloseAndFlowEvents();
    QVERIFY(success);
    QVERIFY(db->eventCount() == 1);

    db->addCloseEventWithBlobString(QLatin1String("{closeEvent}"));
    QVERIFY(db->eventCount() == 2);
    success = db->removeLastCloseAndFlowEvents();
    QVERIFY(success);
    QVERIFY(db->eventCount() == 1);
}

void DatabaseTest::testTransactions()
{
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QString customerId1(QLatin1String("customer1"));
  QString customerId2(QLatin1String("customer2"));
  db->setCustomerId(customerId1);
  QVERIFY(db->customerId() == customerId1);
  
  //start a transaction, change the customer, and see if it's rolled-back:
  QString t(QLatin1String("mytransaction"));
  bool success;
  success = db->beginTransaction(t);
  QVERIFY2(success, "beginTransaction");
  db->setCustomerId(customerId2);
  QVERIFY(db->customerId() == customerId2);
  success = db->rollbackTransaction(t);
  QVERIFY2(success, "rollbackTransaction");

  QVERIFY(db->customerId() == customerId1);

}

void DatabaseTest::testCustomDimensions()
{
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QString dim1(QLatin1String("foo"));
  QString dim2(QLatin1String("bar"));
  QVERIFY(db->setCustomDimension(0, dim1));
  QCOMPARE(db->customDimension(0), dim1);

  QVERIFY(db->setCustomDimension(3, dim2));
  QCOMPARE(db->customDimension(0), dim1);
  QCOMPARE(db->customDimension(3), dim2);

  QVERIFY(db->setCustomDimension(2, QString()));
  QCOMPARE(db->customDimension(2), QString());

  QCOMPARE(db->customDimension(4), QString());
  QCOMPARE(db->customDimension(-1), QString());
}

QTEST_MAIN(DatabaseTest)
#ifdef QMAKE_BUILD
#include "testdatabase.moc"
#else
#include "moc_testdatabase.cxx"
#endif
//#include "tst_database.moc"
