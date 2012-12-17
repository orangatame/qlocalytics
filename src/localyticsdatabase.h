/*
 * Copyright (c) 2012 Orangatame LLC
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met: 
 *  * Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 *
 *  * Neither the name of Orangatame LLC nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY ORANGATAME LLC. ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ORANGATAME LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef LOCALYTICSDATABASE_H
#define LOCALYTICSDATABASE_H

#include <QObject>
#include <QtSql/QSqlDatabase>


class QDateTime;

#define MAX_DATABASE_SIZE   500000  // The maximum allowed disk size of the primary database file at open, in bytes
#define VACUUM_THRESHOLD    0.8     // The database is vacuumed after its size exceeds this proportion of the maximum.


class LocalyticsDatabase : public QObject
{
    Q_OBJECT

      friend class DatabaseTest;
public:
    
    static LocalyticsDatabase* sharedLocalyticsDatabase() {
        if (!_sharedLocalyticsDatabase) {
            _sharedLocalyticsDatabase = new LocalyticsDatabase;
        }
        return _sharedLocalyticsDatabase;
    }
    /*!
      The size of the sqlite3-backing database.
      
      \return Size of the database, in bytes.
     */
    qint64 databaseSize();

    /*!
      Number of events in the database.
      They are added by calling "add*Event*" functions
      \return Pending number of events to send to Localytics.
    */
    int eventCount();

    int unstagedEventCount();

    /*!
      Timestamp when the database file was created.
      
      Should always be valid (whenever there is a database, anyways)
      
      \return Timestamp when the database file was created.
    */
    QDateTime createdTimestamp();

    
    bool incrementLastUploadNumber(int *uploadNumber);
    bool incrementLastSessionNumber(int *sessionNumber);

    bool addEventWithBlobString(QString blob);
    bool addEventWithBlobString(QString blob, int *rowid);

    bool addCloseEventWithBlobString(QString blob);
    bool queueCloseEventWithBlobString(QString blob);
    QString  dequeueCloseEventBlobString();
    bool addFlowEventWithBlobString(QString blob);
    bool removeLastCloseAndFlowEvents();

    bool addHeaderWithSequenceNumber(int number, QString blob, int *insertedRowId);
    bool stageEventsForUpload(int headerId);
    bool updateAppKey(QString appKey);
    QString  uploadBlobString();

    /*!
      Upon successful upload, purges local data that was just uploaded.
      \return `true` on success, `false` otherwise.
    */
    bool deleteUploadedData();
    bool resetAnalyticsData();
    bool vacuumIfRequired();

    QDateTime lastSessionStartTimestamp();
    bool setLastsessionStartTimestamp(QDateTime timestamp);

    bool isOptedOut();
    bool setOptedOut(bool optOut);

    /*!
      Most recent app key-- may not be that used to open the session.
    */
    QString appKey();

    QString customDimension(int dimension);
    bool setCustomDimension(int dimension,  QString value);

    QString customerId();
    bool setCustomerId(QString newCustomerId);

    bool beginTransaction(QString name);
    bool releaseTransaction(QString name);
    bool rollbackTransaction(QString name);


signals:
    
public slots:
    
private:
    /*!
      Default constructor
      \param parent Parent object to retain ownership.
    */
    explicit LocalyticsDatabase(QObject *parent = 0);
    ~LocalyticsDatabase();

    QString pathToDatabaseFile();
    int schemaVersion();
    void createSchema();
    void moveDbToCaches();
    QString randomUUID();
    QSqlDatabase _databaseConnection;

    static LocalyticsDatabase *_sharedLocalyticsDatabase;
};

#endif // LOCALYTICSDATABASE_H
