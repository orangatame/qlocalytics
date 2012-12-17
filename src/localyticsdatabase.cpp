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

#include "localyticsdatabase.h"
#include <QDir>
#include <QtSql/QtSql>
#include <QDebug>
#include <QDateTime>
#include <QUuid>
#include <QChar>
#include <QString>

#define LOCALYTICS_DIR              QLatin1String(".localytics")	// Name for the directory in which Localytics database is stored
#define LOCALYTICS_DB               QLatin1String("localytics")	// File name for the database (without extension)
#define BUSY_TIMEOUT                30              // Maximum time SQlite will busy-wait for the database to unlock before returning SQLITE_BUSY

LocalyticsDatabase* LocalyticsDatabase::_sharedLocalyticsDatabase = 0;


LocalyticsDatabase::LocalyticsDatabase(QObject *parent) : QObject(parent)
{
    // Attempt to open database. It will be created if it does not exist, already.

  _databaseConnection = QSqlDatabase::addDatabase( QLatin1String("QSQLITE") );
  _databaseConnection.setDatabaseName(pathToDatabaseFile());
  bool success = _databaseConnection.open();
  if (!success)
    {
      qDebug() << _databaseConnection.lastError();
      qFatal( "Failed to connect." );
    }

    qDebug( "Connected!" );

//    // If we were unable to open the database, it is likely corrupted. Clobber it and move on.
//    if (code != SQLITE_OK) {
//        [[NSFileManager defaultManager] removeItemAtPath:dbPath error:nil];
//        code =  sqlite3_open([dbPath UTF8String], &_databaseConnection);
//    }

    // Enable foreign key constraints.
    if (success) {
        QSqlQuery q(_databaseConnection);
        success = q.exec(QLatin1String("PRAGMA foreign_keys = ON;"));
    }

    if (schemaVersion() < 7) {
        createSchema();
    }
}

LocalyticsDatabase::~LocalyticsDatabase()
{
  if (_databaseConnection.isOpen()) 
    {
      QSqlDatabase::removeDatabase(QLatin1String("QSQLITE"));
    }
}

QString LocalyticsDatabase::pathToDatabaseFile()
{
  // Create directory structure for Localytics.
  QDir directoryPath = QDir(QDir::homePath() + QLatin1Char('/') + LOCALYTICS_DIR);

  if (!directoryPath.exists()) {
    QDir::home().mkpath(LOCALYTICS_DIR);
  }
  QString p = QDir::homePath() +  QLatin1Char('/') + LOCALYTICS_DIR + QLatin1Char('/') + LOCALYTICS_DB + QLatin1String(".db") ;
  qDebug() << "Path do DB: " << p;
  return p;
}

bool LocalyticsDatabase::beginTransaction(QString name)
{
    QSqlQuery q(_databaseConnection);
    return q.exec(QString(QLatin1String("SAVEPOINT %1")).arg(name));
}

bool LocalyticsDatabase::releaseTransaction(QString name) {
    QSqlQuery q(_databaseConnection);
    return q.exec(QString(QLatin1String("RELEASE SAVEPOINT %1")).arg(name));
}

bool LocalyticsDatabase::rollbackTransaction(QString name) {
    QSqlQuery q(_databaseConnection);
    return q.exec(QString(QLatin1String("ROLLBACK TO SAVEPOINT %1")).arg(name));
}

int LocalyticsDatabase::schemaVersion() {
    int version = 0;
    QSqlQuery query(_databaseConnection);
    query.exec(QLatin1String("SELECT MAX(schema_version) FROM localytics_info"));
    if (query.next()) {
        version = query.value(0).toInt();
    }
    return version;
}

QString LocalyticsDatabase::appKey() {
    QSqlQuery q(_databaseConnection);
    q.exec(QLatin1String("SELECT app_key FROM localytics_info"));
    if (q.next()) {
        return q.value(0).toString();
    }
    return QString();
}


void LocalyticsDatabase::createSchema()
{
    _databaseConnection.transaction();

    // Execute schema creation within a single transaction.
    bool success = true;
    QSqlQuery q(_databaseConnection);

    success &= q.exec(QLatin1String("CREATE TABLE upload_headers ("
                                    "sequence_number INTEGER PRIMARY KEY, "
                                    "blob_string TEXT)"));

    success &= q.exec(QLatin1String("CREATE TABLE events ("
                                    "event_id INTEGER PRIMARY KEY AUTOINCREMENT, " // In case foreign key constraints are reintroduced.
                                    "upload_header INTEGER, "
                                    "blob_string TEXT NOT NULL)"));

    success &= q.exec(QLatin1String("CREATE TABLE localytics_info ("
                                    "schema_version INTEGER PRIMARY KEY, "
                                    "last_upload_number INTEGER, "
                                    "last_session_number INTEGER, "
                                    "opt_out BOOLEAN, "
                                    "last_close_event INTEGER, "
                                    "last_flow_event INTEGER, "
                                    "last_session_start REAL, "
                                    "app_key CHAR(64), "
                                    "customer_id CHAR(64),"
                                    "custom_d0 CHAR(64), "
                                    "custom_d1 CHAR(64), "
                                    "custom_d2 CHAR(64), "
                                    "custom_d3 CHAR(64) "
                                    ")"));

    success &= q.exec(QLatin1String("INSERT INTO localytics_info (schema_version, last_upload_number, last_session_number, opt_out) VALUES (7, 0, 0, 0)"));

    if (success)
        _databaseConnection.commit();
    else
        _databaseConnection.rollback();

}

qint64 LocalyticsDatabase::databaseSize()
{
    QFile db(pathToDatabaseFile());
    return db.size();
}

int LocalyticsDatabase::eventCount() {
    int count = 0;

    QSqlQuery q(_databaseConnection);

    q.exec(QLatin1String("SELECT count(event_id) FROM events"));
    if (q.next()) {
        count = q.value(0).toInt();
    }

    return count;
}

QDateTime LocalyticsDatabase::createdTimestamp()
{
    QFileInfo dbInfo(pathToDatabaseFile());
    return dbInfo.created();
}

QDateTime LocalyticsDatabase::lastSessionStartTimestamp()
{
    QDateTime lastSessionStart;
    QSqlQuery q(_databaseConnection);
    q.exec(QLatin1String("SELECT last_session_start FROM localytics_info"));
    if (q.next()) {
      lastSessionStart.setTime_t(q.value(0).toDouble());
    }

    return lastSessionStart;
}

// Implementation Details:
// If the date is outside the range 1970-01-01T00:00:00 to 2106-02-07T06:28:14, this function returns -1 cast to an unsigned integer (i.e., 0xFFFFFFFF).
bool LocalyticsDatabase::setLastsessionStartTimestamp(QDateTime timestamp) 
{
    QSqlQuery q(_databaseConnection);

    q.prepare(QLatin1String("UPDATE localytics_info SET last_session_start = :last_session"));
    q.bindValue(QLatin1String(":last_session"), timestamp.toTime_t());
    return q.exec();
}

bool LocalyticsDatabase::isOptedOut() {
    bool optedOut = false;
    QSqlQuery q(_databaseConnection);
    q.exec(QLatin1String("SELECT opt_out FROM localytics_info"));
    if (q.next()) {
        optedOut = q.value(0).toBool();
    }
    return optedOut;
}

bool LocalyticsDatabase::setOptedOut(bool optOut)
{
    QSqlQuery q(_databaseConnection);
    q.prepare(QLatin1String("UPDATE localytics_info SET opt_out = :opted_out"));
    q.bindValue(QLatin1String(":opted_out"), optOut);
    return q.exec();
}

QString LocalyticsDatabase::customDimension(int dimension)
{
  if(dimension < 0 || dimension > 3) 
    {
      return QString();
    }
  
  QSqlQuery q(_databaseConnection);
  q.exec(QString(QLatin1String("SELECT custom_d%1 FROM localytics_info")).arg(dimension));
  
  if (q.next())
    {
      return q.value(0).toString();
    }
  return QString();
}


bool LocalyticsDatabase::setCustomDimension(int dimension, QString value)
{
  if(dimension < 0 || dimension > 3) {
    return false;
  }
  QSqlQuery q(_databaseConnection);
  q.prepare(QString(QLatin1String("UPDATE localytics_info SET custom_d%1 = :value")).arg(dimension));

  q.bindValue(QLatin1String(":value"), value);
  return q.exec();
}

bool LocalyticsDatabase::incrementLastUploadNumber(int *uploadNumber)
{
    QString t(QLatin1String("increment_upload_number"));
    bool success = true;

    success = beginTransaction(t);
    QSqlQuery q(_databaseConnection);

    if (success)
      {
        // Increment value
        success = q.exec(QLatin1String("UPDATE localytics_info "
                                       "SET last_upload_number = (last_upload_number + 1)"));
      }

    if (success)
      {
        // Retrieve new value
        success = q.exec(QLatin1String("SELECT last_upload_number FROM localytics_info"));
        if (q.next()) 
          {
            *uploadNumber = q.value(0).toInt();
          }
      }

    if (success) 
      {
        releaseTransaction(t);
      }
    else
      {
        rollbackTransaction(t);
      }
    return success;
}

bool LocalyticsDatabase::incrementLastSessionNumber(int *sessionNumber)
{
    QString t(QLatin1String("increment_session_number"));
    bool success = true;

    success = beginTransaction(t);
    QSqlQuery q(_databaseConnection);

    if (success)
      {
        // Increment value
        success = q.exec(QLatin1String("UPDATE localytics_info "
                                       "SET last_session_number = (last_session_number + 1)"));
      }

    if (success) 
      {
        // Retrieve new value
        success = q.exec(QLatin1String("SELECT last_session_number FROM localytics_info"));
        if (q.next()) 
          {
            *sessionNumber = q.value(0).toInt();
          }
      }

    if (success) 
      {
        releaseTransaction(t);
      } 
    else
      {
        rollbackTransaction(t);
      }
    return success;
}

bool LocalyticsDatabase::addEventWithBlobString(QString blob, int *rowid)
{
    QSqlQuery q(_databaseConnection);
    q.prepare(QLatin1String("INSERT INTO events (blob_string) VALUES (:blob_string)"));
    q.bindValue(QLatin1String(":blob_string"), blob);
    bool success = q.exec();
    if (success && rowid != NULL)
      {
        *rowid = q.lastInsertId().toInt();
      }
    return success;
}

bool LocalyticsDatabase::addEventWithBlobString(QString blob)
{
  return addEventWithBlobString(blob, 0);
}

bool LocalyticsDatabase::addCloseEventWithBlobString(QString blob)
{
    QString t(QLatin1String("add_close_event"));
    bool success = beginTransaction(t);

    int event_id;
    // Add close event.
    if (success) 
      {
        success = addEventWithBlobString(blob, &event_id);
      }

    // Record row id to localytics_info so that it can be removed if the session resumes.
    if (success) {
        QSqlQuery q(_databaseConnection);
        q.prepare(QLatin1String("UPDATE localytics_info SET last_close_event = (SELECT event_id FROM events WHERE rowid = :rowid)"));
        q.bindValue(QLatin1String(":rowid"), event_id);
        success = q.exec();
    }

    if (success) {
        releaseTransaction(t);
    } else {
        rollbackTransaction(t);
    }
    return success;
}

bool LocalyticsDatabase::queueCloseEventWithBlobString(QString blob)
{
    QString t(QLatin1String("queue_close_event"));
    bool success = this->beginTransaction(t);

    // Queue close event.
    if (success) {
        QSqlQuery queueCloseEvent(_databaseConnection);
        queueCloseEvent.prepare(QLatin1String("UPDATE localytics_info SET queued_close_event_blob = :blob"));
        queueCloseEvent.bindValue(QLatin1String(":blob"), blob);
        success = queueCloseEvent.exec();
    }
    if (success) {
        this->releaseTransaction(t);
    } else {
        this->rollbackTransaction(t);
    }
    return success;
}

QString LocalyticsDatabase::dequeueCloseEventBlobString()
{
    QString val;
    QSqlQuery q(_databaseConnection);
    q.exec(QLatin1String("SELECT queued_close_event_blob FROM localytics_info"));
    if (q.next()) {
        val = q.value(0).toString();
    }

    // Clear the queued close event blob.
    queueCloseEventWithBlobString(QString());

    return val;
}

bool LocalyticsDatabase::addFlowEventWithBlobString(QString blob)
{
    QString t(QLatin1String("add_flow_event"));
    bool success = this->beginTransaction(t);
    int event_id;
    // Add flow event.
    if (success) {
      success = addEventWithBlobString(blob, &event_id);
    }

    // Record row id to localytics_info so that it can be removed if the session resumes.
    if (success) {
        QSqlQuery q(_databaseConnection);
        q.prepare(QLatin1String("UPDATE localytics_info SET last_flow_event = (SELECT event_id FROM events WHERE rowid = :rowid)"));
        q.bindValue(QLatin1String(":rowid"), event_id);
        success = q.exec();
    }

    if (success) {
        this->releaseTransaction(t);
    } else {
        this->rollbackTransaction(t);
    }
    return success;
}

bool LocalyticsDatabase::removeLastCloseAndFlowEvents()
{
    // Attempt to remove the last recorded close event.
    // Fail quietly if none was saved or it was previously removed.
    QSqlQuery q(_databaseConnection);

    return q.exec(QLatin1String("DELETE FROM events WHERE event_id = (SELECT last_close_event FROM localytics_info) OR event_id = (SELECT last_flow_event FROM localytics_info)"));
}

bool LocalyticsDatabase::addHeaderWithSequenceNumber(int number, QString blob, int *insertedRowId)
{
    QSqlQuery q(_databaseConnection);
    q.prepare(QLatin1String("INSERT INTO upload_headers (sequence_number, blob_string) VALUES (:sequence, :blob)"));
    q.bindValue(QLatin1String(":sequence"), number);
    q.bindValue(QLatin1String(":blob"), blob);
    bool success = q.exec();
    if (success && insertedRowId != NULL) {
        *insertedRowId = q.lastInsertId().toInt();
    }

    return success;
}

int LocalyticsDatabase::unstagedEventCount()
{
    int rowCount = 0;
    QSqlQuery q(_databaseConnection);
    q.exec(QLatin1String("SELECT COUNT(*) FROM events WHERE UPLOAD_HEADER IS NULL"));
    if (q.next()) {
        rowCount = q.value(0).toInt();
    }
    return rowCount;
}

bool LocalyticsDatabase::stageEventsForUpload(int headerId)
{
    // Associate all outstanding events with the given upload header ID.
    QSqlQuery q(_databaseConnection);
    q.prepare(QLatin1String("UPDATE events SET upload_header = :upload_header WHERE upload_header IS NULL"));
    q.bindValue(QLatin1String(":upload_header"), headerId);

    return q.exec();
}

bool LocalyticsDatabase::updateAppKey(QString appKey)
{
    QSqlQuery q(_databaseConnection);
    q.prepare(QLatin1String("UPDATE localytics_info set app_key = :app_key"));
    q.bindValue(QLatin1String(":app_key"), appKey);
    return q.exec();
}

QString LocalyticsDatabase::uploadBlobString()
{
    // Retrieve the blob strings of each upload header and its child events, in order.
    QSqlQuery q(_databaseConnection);
    q.exec(QLatin1String("SELECT * FROM ( "
                         "   SELECT h.blob_string AS 'blob', h.sequence_number as 'seq', 0 FROM upload_headers h"
                         "   UNION ALL "
                         "   SELECT e.blob_string AS 'blob', e.upload_header as 'seq', 1 FROM events e"
                         ") "
                         "ORDER BY 2, 3"));
    QString uploadBlobString;
    while (q.next()) {
        QString blob = q.value(0).toString();
        uploadBlobString += blob;
    }

    return uploadBlobString;
}

bool LocalyticsDatabase::deleteUploadedData()
{
    // Delete all headers and staged events.
    QString t(QLatin1String("delete_upload_data"));
    bool success = beginTransaction(t);

    QSqlQuery q(_databaseConnection);
    success &= q.exec(QLatin1String("DELETE FROM events WHERE upload_header IS NOT NULL"));
    success &= q.exec(QLatin1String("DELETE FROM upload_headers"));

    if (success) {
        releaseTransaction(t);
    } else {
        rollbackTransaction(t);
    }
    return success;
}

bool LocalyticsDatabase::resetAnalyticsData() {
    // Delete or zero all analytics data.
    // Reset: headers, events, session number, upload number, last session start, last close event, and last flow event.
    // Unaffected: schema version, opt out status, install ID (deprecated), and app key.

    QString t(QLatin1String("reset_analytics_data"));
    bool success = beginTransaction(t);
    QSqlQuery q(_databaseConnection);

    success &= q.exec(QLatin1String("DELETE FROM events"));
    success &= q.exec(QLatin1String("DELETE FROM upload_headers"));
    success &= q.exec(QLatin1String("DELETE FROM localytics_amp_rule"));
    success &= q.exec(QLatin1String("DELETE FROM localytics_amp_ruleevent"));
    success &= q.exec(QLatin1String("UPDATE localytics_info SET "
                                    " last_session_number = 0, last_upload_number = 0,"
                                    " last_close_event = null, last_flow_event = null, last_session_start = null, "
                                    " custom_d0 = null, custom_d1 = null, custom_d2 = null, custom_d3 = null, "
                                    " customer_id = null, queued_close_event_blob = null "));
    if (success) {
        releaseTransaction(t);
    } else {
        rollbackTransaction(t);
    }

    return success;
}

bool LocalyticsDatabase::vacuumIfRequired()
{
    if (this->databaseSize() > MAX_DATABASE_SIZE * VACUUM_THRESHOLD) 
      {
        QSqlQuery q(_databaseConnection);
        return q.exec(QLatin1String("VACUUM"));
      }
    return true;
}

QString LocalyticsDatabase::randomUUID() {
    QUuid uu = QUuid::createUuid();
    return uu.toString();
}

QString LocalyticsDatabase::customerId()
{
   QString customerId;

   QSqlQuery q(_databaseConnection);
   q.exec(QLatin1String("SELECT customer_id FROM localytics_info"));
   if (q.next()) {
       customerId = q.value(0).toString();
   }
   return customerId;
}

bool LocalyticsDatabase::setCustomerId(QString newCustomerId)
{
    QSqlQuery q(_databaseConnection);
    q.prepare(QLatin1String("UPDATE localytics_info set customer_id = :customer_id"));
    q.bindValue(QLatin1String(":customer_id"), newCustomerId);
    return q.exec();
}

