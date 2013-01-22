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

#include "localyticssession.h"
#include "localyticsdatabase.h"
#include "localyticsuploader.h"
#include "webserviceconstants.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QRegExp>
#include <QSettings>
#include <QUuid>
#include <QVariantMap>

#ifdef __QNXNTO__
#include <bb/device/HardwareInfo>
#include <bb/device/CellularRadioInfo>
#include <bb/device/SdCardInfo>
#include <bb/MemoryInfo>
#endif

// The randomly generated ID for each install of the app
#define PREFERENCES_KEY QLatin1String("_localytics_install_id")

// The version of this library
#define CLIENT_VERSION  QLatin1String("iOS_2.14")

// The directory in which the Localytics database is stored
#define LOCALYTICS_DIR  QLatin1String(".localytics")

// Ethernet CSMACD
#define IFT_ETHER       0x6

// Default value for how many seconds a session persists when App
// shifts to the background.
#define DEFAULT_BACKGROUND_SESSION_TIMEOUT 15   


LocalyticsSession* LocalyticsSession::_sharedLocalyticsSession = 0;

LocalyticsSession::LocalyticsSession(QObject *parent) : QObject(parent)
{
        _isSessionOpen  = false;
        _hasInitialized = false;
        _backgroundSessionTimeout = DEFAULT_BACKGROUND_SESSION_TIMEOUT;
        _sessionHasBeenOpen = false;
        _enableHTTPS = true;

        LocalyticsDatabase::sharedLocalyticsDatabase();
}

void LocalyticsSession::init(QString appKey)
{
  // If the session has already initialized, don't bother doing it again.
  if (hasInitialized())
    {
      logMessage(QLatin1String("Object has already been initialized."));
      return;
    }
  if (appKey.isNull() || appKey.length() == 0)
    {
      logMessage(QLatin1String("App key is null or empty."));
      _hasInitialized = false;
      return;
    }

  // App key should only be alphanumeric chars and dashes.
  //NSString *trimmedAppKey = [appKey stringByReplacingOccurrencesOfString:@"-" withString:@""];
  //if([[trimmedAppKey stringByTrimmingCharactersInSet:[NSCharacterSet alphanumericCharacterSet]] isEqualToString:@""] == false) {
  //  [self logMessage:@"App key may only contain dashes and alphanumeric characters."];
  //  self.hasInitialized = NO;
  //  return;
  //}
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  if (db) {
    // Check if the app key has changed.
    QString lastAppKey = db->appKey();
    if (lastAppKey != appKey) {
      if (!lastAppKey.isEmpty()) {
        // Clear previous events and dimensions to guarantee that new data
        // isn't associated with the old app key.
        db->resetAnalyticsData();

        // Vacuum to improve the odds of opening a new session following bulk
        // delete.
        db->vacuumIfRequired();
      }
      // Record the key for future checks.
      db->updateAppKey(appKey);
    }

    _applicationKey = appKey;
    _hasInitialized = true;
    logMessage(QLatin1String("Object Initialized. Application's key is: ") + _applicationKey);
  }
}

void LocalyticsSession::startSession(QString appKey)
{
  QRegExp r(QLatin1String("[A-Fa-f0-9-]+"));
  if (!r.exactMatch(appKey))
    {
      //throw exception
      //NSException *exception = [NSException exceptionWithName:@"Invalid Localytics App Key" reason:@"Application key is not valid. Please look at the iOS integration guidlines at http://www.localytics.com/docs/iphone-integration/" userInfo:nil];
      //[exception raise];
    }
  this->init(appKey);
  this->open();
  this->upload();
}

// Public interface to ll_open.
void LocalyticsSession::open()
{
  //dispatch_async(_queue, ^{
  ll_open();
  //  });
}

bool LocalyticsSession::resume()
{
  // Do nothing if session is already open
  if (_isSessionOpen == true)
    return true;

  bool ret = false;
  // conditions for resuming previous session
  QDateTime now = QDateTime::currentDateTime();
  if (_sessionHasBeenOpen &&
      (!_sessionCloseTime.isValid() || _sessionCloseTime.secsTo(now) <= _backgroundSessionTimeout)
      ) 
    {
      // Note that we allow the session to be resumed even if the
      // database size exceeds the maximum. This is because we don't
      // want to create incomplete sessions. If the DB was large enough
      // that the previous session could not be opened, there will be
      // nothing to resume. But if this session caused it to go over it
      // is better to let it complete and stop the next one from being
      // created.
      ret = true;
      if(ll_isOptedIn() == false) 
        {
          logMessage(QLatin1String("Can't resume session because user is opted out."));
        } 
      else
        {
          logMessage(QLatin1String("Resume called - Resuming previous session."));
          reopenPreviousSession();
        }
    } 
  else 
    {
      ret = false;
    if (ll_isOptedIn() == false) 
      {
        logMessage(QLatin1String("Can't resume session because user is opted out."));
      } 
    else
      {
        // otherwise open new session and upload
        logMessage(QLatin1String("Resume called - Opening a new session."));
        ll_open();
      }
    }
  _sessionCloseTime = QDateTime();
  return ret;
}

void LocalyticsSession::close()
{
  // Do nothing if the session is not open
  if (_isSessionOpen == false) 
    {
      logMessage(QLatin1String("Unable to close session - session is not open!"));
      return;
    }

  // Save time of close
  _sessionCloseTime = QDateTime::currentDateTime();

  // Update active session duration.
  _sessionActiveDuration += _sessionResumeTime.secsTo(_sessionCloseTime);

  int sessionLength = (int)(QDateTime::currentDateTime().toTime_t() - _lastSessionStartTimestamp.toTime_t());

  //try {
  // Create the JSON representing the close blob
  QString closeEventString;
  closeEventString.append(QLatin1Char('{'));
  closeEventString.append(formatAttribute(PARAM_DATA_TYPE, QLatin1String("c"), true));
  closeEventString.append(formatAttribute(PARAM_SESSION_UUID, _sessionUUID));
  closeEventString.append(formatAttribute(PARAM_UUID, randomUUID()));
  closeEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_SESSION_START).arg((double) _lastSessionStartTimestamp.toTime_t(), 0, 'f', 0));
  closeEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_SESSION_ACTIVE).arg(_sessionActiveDuration));
  closeEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_CLIENT_TIME).arg((double) QDateTime::currentDateTime().toTime_t(), 0, 'f', 0));

  // Avoid recording session lengths of users with unreasonable client
  // times (usually caused by developers testing clock change attacks)
  if (sessionLength > 0 && sessionLength < 400000)
    {
      closeEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_SESSION_TOTAL).arg(sessionLength));
    }

  // Open second level - screen flow
  closeEventString.append(QString(QLatin1String(",\"%1\":[")).arg(PARAM_SESSION_SCREENFLOW));
  closeEventString.append(_screens);
  // Close second level - screen flow
  closeEventString.append(QLatin1Char(']'));

  // Append the custom dimensions
  closeEventString.append(customDimensions());

  // Append the location
  closeEventString.append(locationDimensions());

  // Close first level - close blob
  closeEventString.append(QLatin1String("}\n"));

  bool success = LocalyticsDatabase::sharedLocalyticsDatabase()->queueCloseEventWithBlobString(closeEventString);

  _isSessionOpen = false;  // Session is no longer open.

  if (success)
    {
      logMessage(QLatin1String("Session successfully closed."));
    }
  else
    {
      logMessage(QLatin1String("Failed to record session close."));
    }
}

void LocalyticsSession::setOptIn(bool optedIn)
{
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QString t(QLatin1String("set_opt"));
  bool success = db->beginTransaction(t);

  // Write out opt event.
  if (success)
    {
      success = this->createOptEvent(optedIn);
    }

  // Update database with the option (stored internally as an opt-out).
  if (success)
    {
      db->setOptedOut(optedIn == false);
    }

  if (success && optedIn == false)
    {
      // Disable all further Localytics calls for this and future sessions
      // This should not be flipped when the session is opted back in because that
      // would create an incomplete session.
      _isSessionOpen = false;
    }

  if (success)
    {
      db->releaseTransaction(t);
      logMessage(QString(QLatin1String("Application opted %1")).arg(optedIn ? QLatin1String("in") : QLatin1String("out")));
    }
  else
    {
      db->rollbackTransaction(t);
      logMessage(QLatin1String("Failed to update opt state."));
    }

}

bool LocalyticsSession::isOptedIn()
{
  return this->ll_isOptedIn();
}

void LocalyticsSession::tagEvent(const QString &event)
{
	tagEvent(event, QVariantMap());
}

void LocalyticsSession::tagEvent(const QString &event, const QVariantMap &attributes)
{
	tagEvent(event, attributes, QVariantMap());
}

void LocalyticsSession::tagEvent(const QString &event, const QVariantMap &attributes, const QVariantMap &reportAttributes)
{
	// Do nothing if the session is not open.
	if (_isSessionOpen == false)
	{
          logMessage(QLatin1String("Cannot tag an event because the session is not open."));
		return;
	}

	if(event.isEmpty())
	{
          logMessage(QLatin1String("Event tagged without a name. Skipping."));
		return;
	}

	// Create the JSON for the event
	QString eventString;
	eventString.append(QLatin1Char('{'));
	eventString.append(formatAttribute(PARAM_DATA_TYPE,  QLatin1String("e"), true));
	eventString.append(formatAttribute(PARAM_UUID, randomUUID()));
	eventString.append(formatAttribute(PARAM_APP_KEY, _applicationKey));
	eventString.append(formatAttribute(PARAM_SESSION_UUID, _sessionUUID));
	eventString.append(formatAttribute(PARAM_EVENT_NAME, escapeString(event)));
	eventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_CLIENT_TIME).arg((double) QDateTime::currentDateTime().toTime_t(), 0, 'f', 0));

	// Append the custom dimensions
	eventString.append(customDimensions());

	// Append the location
	eventString.append(locationDimensions());

	// If there are any attributes for this event, add them as a hash
	int attrIndex = 0;
	if(!attributes.isEmpty())
	{
		// Open second level - attributes
                eventString.append(QString(QLatin1String(",\"%1\":{")).arg(PARAM_ATTRIBUTES));

		 QMapIterator<QString, QVariant> i(attributes);
		 while (i.hasNext())
		 {
			 i.next();
			 // Have to escape paramName and paramValue because they user-defined.

			 eventString.append(formatAttribute(escapeString(i.key()), escapeString(i.value().toString()), attrIndex == 0));
			 attrIndex++;
		 }

		// Close second level - attributes
		eventString.append(QLatin1Char('}'));
	}

	// If there are any report attributes for this event, add them as above
	attrIndex = 0;
	if (!reportAttributes.isEmpty())
          {
                eventString.append(QString(QLatin1String(",\"%1\":{")).arg(PARAM_REPORT_ATTRIBUTES));
		QMapIterator<QString, QVariant> i(reportAttributes);
		while (i.hasNext())
                  {
			i.next();
			eventString.append(formatAttribute(escapeString(i.key()), escapeString(i.value().toString()), attrIndex == 0));
			attrIndex++;
		}
		eventString.append(QLatin1Char('}'));
	}

	// Close first level - Event information
	eventString.append(QLatin1String("}\n"));

	bool success = LocalyticsDatabase::sharedLocalyticsDatabase()->addEventWithBlobString(eventString);
	if (success) 
          {
            // User-originated events should be tracked as application flow.
            addFlowEvent(event, QLatin1String("e")); // "e" for Event.
            logMessage(QLatin1String("Tagged event: ") + event);
          } 
        else
          {
            logMessage(QLatin1String("Failed to tag event."));
          }

}

void LocalyticsSession::upload()
{
  if (LocalyticsUploader::sharedLocalyticsUploader()->isUploading())
    {
      logMessage(QLatin1String("An upload is already in progress. Aborting."));
      return;
    }

  QString t(QLatin1String("stage_upload"));
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  bool success = db->beginTransaction(t);

  // - The event list for the current session is not modified

  // New flow events are only transitioned to the "old" list if the
  // upload is staged successfully. The queue ensures that the list of
  // events are not modified while a call to upload is in progress.
  if (success)
    {
      // Write flow blob to database. This is for a session in
      // progress and should not be removed upon resume.
      success = saveApplicationFlowAndRemoveOnResume(false);
    }

  if (success && db->unstagedEventCount() > 0)
    {
      // Increment upload sequence number.
      int sequenceNumber = 0;
      success = db->incrementLastUploadNumber(&sequenceNumber);

      // Write out header to database.
      int headerRowId = 0;
      if (success) 
        {
          QString headerBlob = blobHeaderStringWithSequenceNumber(sequenceNumber);
          success = db->addHeaderWithSequenceNumber(sequenceNumber, headerBlob, &headerRowId);
        }

      // Associate unstaged events.
      if (success)
        {
        success = db->stageEventsForUpload(headerRowId);
        }
    }

  if (success)
    {
      // Complete transaction
      db->releaseTransaction(t);
      
      // Move new flow events to the old flow event array.
      if (_unstagedFlowEvents.length() > 0) 
        {
          if (_stagedFlowEvents.length() > 0) 
            {
              _stagedFlowEvents.append(QLatin1Char(',')).append(_unstagedFlowEvents);
            } 
          else
            {
              _stagedFlowEvents = _unstagedFlowEvents;
            }
          _unstagedFlowEvents = QString(QLatin1String(""));
        }
      
      // Begin upload.
      LocalyticsUploader::sharedLocalyticsUploader()->upload(_applicationKey, _enableHTTPS,  installationId());
    }
  else
    {
      db->rollbackTransaction(t);
      logMessage(QLatin1String("Failed to start upload."));
    }
}

QString LocalyticsSession::libraryVersion()
{
  return CLIENT_VERSION;
}

void LocalyticsSession::dequeueCloseEventBlobString()
{
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QString closeEventString = db->dequeueCloseEventBlobString();
  if (!closeEventString.isNull() && !closeEventString.isEmpty())
    {
      bool success = db->addCloseEventWithBlobString(closeEventString);
      if (!success)
        {
          // Re-queue the close event.
          db->queueCloseEventWithBlobString(closeEventString);
        }
    }
}

void LocalyticsSession::ll_open()
{
  // There are a number of conditions in which nothing should be done:
  if (_hasInitialized == false || // the session object has not yet initialized
      _isSessionOpen == true)  // session has already been opened
    {
      logMessage(QLatin1String("Unable to open session."));
      return;
    }

  if (ll_isOptedIn() == false)
    {
      logMessage(QLatin1String("Can't open session because user is opted out."));
      return;
    }
  //TRY
  // If there is too much data on the disk, don't bother collecting any more.
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  if (db->databaseSize() > MAX_DATABASE_SIZE) 
    {
      logMessage(QLatin1String("Database has exceeded the maximum size. Session not opened."));
      _isSessionOpen = false;
      return;
    }

  this->dequeueCloseEventBlobString();

  _sessionActiveDuration = 0;
  _sessionResumeTime = QDateTime::currentDateTime();
  _unstagedFlowEvents = QString();
  _stagedFlowEvents = QString();
  _screens = QString();

  // Begin transaction for session open.
  QString t(QLatin1String("open_session"));
  bool success = db->beginTransaction(t);

  // lastSessionStartTimestamp isn't really the last session start time.
  // It's the sessionResumeTime which is [NSDate date] or now. Therefore,
  // save the current lastSessionTimestamp value from the database so it
  // can be used to calculate the elapsed time between session start times.
  QDateTime previousSessionStartTime = db->lastSessionStartTimestamp();

  // Save session start time.
  _lastSessionStartTimestamp = _sessionResumeTime;
  if (success) 
    {
      success = db->setLastsessionStartTimestamp(_lastSessionStartTimestamp);
    }

  // Retrieve next session number.
  int sessionNumber = 0;
  if (success) 
    {
      success = db->incrementLastSessionNumber(&sessionNumber);
    }
  
  _sessionNumber = sessionNumber;

  if (success) 
    {
      // Prepare session open event.
      _sessionUUID = this->randomUUID();
      
      // Store event.
      QString openEventString;
      openEventString.append(QLatin1Char('{'));
      openEventString.append(this->formatAttribute(PARAM_DATA_TYPE, QLatin1String("s"), true));
      openEventString.append(this->formatAttribute(PARAM_NEW_SESSION_UUID, _sessionUUID));
      openEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_CLIENT_TIME).arg((double) _lastSessionStartTimestamp.toTime_t(), 0, 'f', 0)); // measured in seconds.
      openEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_SESSION_NUMBER).arg(sessionNumber));

      double elapsedTime = 0.0;
      if (previousSessionStartTime.isValid())
        {
          elapsedTime = _lastSessionStartTimestamp.toTime_t() - previousSessionStartTime.toTime_t();
          Q_ASSERT(elapsedTime >= 0);
        }
      QString elapsedTimeString = QString(QLatin1String("%1")).arg(elapsedTime, 0, 'f', 0);
      openEventString.append(this->formatAttribute(PARAM_SESSION_ELAPSE_TIME, elapsedTimeString));
      
      openEventString.append(this->customDimensions());
      openEventString.append(this->locationDimensions());
      
      openEventString.append(QLatin1String("}\n"));
      success = db->addEventWithBlobString(openEventString);
    }

  if (success)
    {
      db->releaseTransaction(t);
      _isSessionOpen = true;
      _sessionHasBeenOpen = true;
      logMessage(QLatin1String("Successfully opened session. UUID is: ") + _sessionUUID);
    }
  else
    {
      db->rollbackTransaction(t);
      _isSessionOpen = false;
      logMessage(QLatin1String("Failed to open session."));
    }

}

/*!
 @method reopenPreviousSession
 @abstract Reopens the previous session, using previous session variables. If there was no previous session, do nothing.
*/
void LocalyticsSession::reopenPreviousSession()
{
  if (_sessionHasBeenOpen == false)
    {
      logMessage(QLatin1String("Unable to reopen previous session, because a previous session was never opened."));
      return;
    }

  // Record session resume time.
  _sessionResumeTime = QDateTime::currentDateTime();

  //Remove close and flow events if they exist.
  LocalyticsDatabase::sharedLocalyticsDatabase()->removeLastCloseAndFlowEvents();
  _isSessionOpen = true;
}

/*!
 @method addFlowEvent
 @abstract Adds a simple key-value pair to the list of events tagged during this session.
 @param name The name of the tagged event.
 @param eventType A key representing the type of the tagged event. Either "s" for Screen or "e" for Event.
 */
void LocalyticsSession::addFlowEvent(const QString &name, const QString& eventType)
{
    if (name.isEmpty() || eventType.isEmpty())
        return;
//
//    // Format new event as simple key-value dictionary.
//    QString eventString = formatAttribute(eventType, escapeString(name), true);
//
//    // Flow events are uploaded as a sequence of key-value pairs. Wrap the above in braces and append to the list.
//    BOOL previousFlowEvents = self.unstagedFlowEvents.length > 0;
//    if (previousFlowEvents) {
//        [self.unstagedFlowEvents appendString:@","];
//    }
//    [self.unstagedFlowEvents appendFormat:@"{%@}", eventString];
}


/*!
 @method blobHeaderStringWithSequenceNumber:
 @abstract Creates the JSON string for the upload blob header, substituting in the given upload sequence number.
 @param  nextSequenceNumber The sequence number for the current upload attempt.
 @return The upload header JSON blob.
 */
QString LocalyticsSession::blobHeaderStringWithSequenceNumber(int nextSequenceNumber)
{
  QString headerString;

  QString uuid = this->randomUUID();
  QString device_uuid = this->uniqueDeviceIdentifier();
  QLocale locale = QLocale::system();
  QString device_language = QLocale::languageToString(locale.language());
  QString locale_country = QLocale::countryToString(locale.country());

  // Open first level - blob information
  headerString.append(QLatin1Char('{'));
  headerString.append(QString(QLatin1String("\"%1\":%2")).arg(PARAM_SEQUENCE_NUMBER).arg(nextSequenceNumber));
  headerString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_PERSISTED_AT).arg((double) LocalyticsDatabase::sharedLocalyticsDatabase()->createdTimestamp().toTime_t(),0,'f', 0));
  headerString.append(formatAttribute(PARAM_DATA_TYPE, QLatin1String("h")));
  headerString.append(formatAttribute(PARAM_UUID, uuid));

  // Open second level - blob header attributes
  headerString.append(QString(QLatin1String(",\"%1\":{")).arg(PARAM_ATTRIBUTES));
  headerString.append(formatAttribute(PARAM_DATA_TYPE, QLatin1String("a"), true));

  // >>  Application and session information
  headerString.append(formatAttribute(PARAM_INSTALL_ID, installationId()));
  headerString.append(formatAttribute(PARAM_APP_KEY, _applicationKey));
  headerString.append(formatAttribute(PARAM_APP_VERSION, appVersion()));
  headerString.append(formatAttribute(PARAM_LIBRARY_VERSION, libraryVersion()));

  // >>  Device Information
  if (!device_uuid.isEmpty())
  {
    headerString.append(formatAttribute(PARAM_DEVICE_UUID_HASHED, hashString(device_uuid)));
  }

  headerString.append(formatAttribute(PARAM_DEVICE_MANUFACTURER, QLatin1String("RIM")));
  headerString.append(formatAttribute(PARAM_DEVICE_PLATFORM, QLatin1String("BlackBerry10")));
  headerString.append(formatAttribute(PARAM_DEVICE_OS_VERSION, systemVersion()));
  headerString.append(formatAttribute(PARAM_DEVICE_MODEL, deviceModel()));
  headerString.append(formatAttribute(PARAM_DATA_CONNECTION_TYPE, getNetworkType()));

  qint64 availableMemoryBytes = availableMemory();
  if (availableMemoryBytes > 0)
    {
      headerString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_DEVICE_MEMORY).arg(availableMemoryBytes));
    }
  headerString.append(formatAttribute(PARAM_LOCALE_LANGUAGE, device_language));
  headerString.append(formatAttribute(PARAM_LOCALE_COUNTRY, locale_country));
  //headerString.append(formatAttribute(PARAM_DEVICE_COUNTRY, [locale objectForKey:NSLocaleCountryCode]));
  headerString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_JAILBROKEN).arg(isDeviceJailbroken() ? QLatin1String("true") : QLatin1String("false")));

  //  Close second level - attributes
  headerString.append(QLatin1Char('}'));

  headerString.append(QLatin1Char('}'));
  return headerString;
}

bool LocalyticsSession::ll_isOptedIn()
{
  return LocalyticsDatabase::sharedLocalyticsDatabase()->isOptedOut() == false;
}

/*!
 @method createOptEvent:
 @abstract Generates the JSON for an opt event (user opting in or out) and writes it to the database.
 @return <c>true</c> if the event was written to the database, <c>false</c> otherwise
 */
bool LocalyticsSession::createOptEvent(bool optState)
{
  QString optEventString;
  optEventString.append(QLatin1Char('{'));
  optEventString.append(formatAttribute(PARAM_DATA_TYPE, QLatin1String("o"), true));
  optEventString.append(formatAttribute(PARAM_UUID, randomUUID()));

  //this actually transmits the opposite of the opt state. The JSON contains whether the user is opted out, not whether the user is opted in.
  optEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_OPT_VALUE).arg(optState ? QLatin1String("false") : QLatin1String("true")));

  optEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_CLIENT_TIME).arg((double) QDateTime::currentDateTime().toTime_t(), 0, 'f', 0));
  optEventString.append(QLatin1String("}\n"));

  bool success = LocalyticsDatabase::sharedLocalyticsDatabase()->addEventWithBlobString(optEventString);
  return success;
}

/*
  @method saveApplicationFlowAndRemoveOnResume:
  @abstract Constructs an application flow blob string and writes it to the database, optionally flagging it for deletion
  if the session is resumed.
  @param removeOnResume YES if the application flow blob should be deleted if the session is resumed.
  @return YES if the application flow event was written to the database successfully.
*/
bool LocalyticsSession::saveApplicationFlowAndRemoveOnResume(bool removeOnResume)
{
  bool success = true;
  
  // If there are no new events, then there is nothing additional to save.
  if (_unstagedFlowEvents.length())
    {
      // Flows are uploaded as a distinct blob type containing
      // arrays of new and previously-uploaded event and screen
      // names. Write a flow event to the database.
      QString flowEventString;
      
      // Open first level - flow blob event
      flowEventString.append(QLatin1Char('{'));
      flowEventString.append(formatAttribute(PARAM_DATA_TYPE, QLatin1String("f"), true));
      flowEventString.append(formatAttribute(PARAM_UUID, randomUUID()));
      flowEventString.append(QString(QLatin1String(",\"%1\":%2")).arg(PARAM_SESSION_START).arg((double) _lastSessionStartTimestamp.toTime_t(), 0, 'f', 0));
      
      // Open second level - new flow events
      flowEventString.append(QString(QLatin1String(",\"%1\":[")).arg(PARAM_NEW_FLOW_EVENTS));
      flowEventString.append(_unstagedFlowEvents); // Flow events are escaped in |-addFlowEventWithName:|
      // Close second level - new flow events
      flowEventString.append(QLatin1Char(']'));
      
      // Open second level - old flow events
      flowEventString.append(QString(QLatin1String(",\"%1\":[")).arg(PARAM_OLD_FLOW_EVENTS));
      flowEventString.append(_stagedFlowEvents);
      // Close second level - old flow events
      flowEventString.append(QLatin1Char(']'));
      
      // Close first level - flow blob event
      flowEventString.append(QString(QLatin1String("}\n")));
      
      success = LocalyticsDatabase::sharedLocalyticsDatabase()->addFlowEventWithBlobString(flowEventString);
    }
  return success;
}


QString LocalyticsSession::formatAttribute(QString name, QString value)
{
  return formatAttribute(name, value, false);
}

QString LocalyticsSession::formatAttribute(QString name, QString value, bool firstAttribute)
{
  QString formattedString;
  if (!firstAttribute)
    {
      formattedString.append(QLatin1Char(','));
    }
  QString quotedName = QString(QLatin1String("\"%1\"")).arg(name);
  value = value.isEmpty() ? QString(QLatin1String("null")) : QString(QLatin1String("\"%1\"")).arg(value);
  formattedString.append(quotedName).append(QLatin1Char(':')).append(value);
  return formattedString;
}

/*!
 @method escapeString
 @abstract Formats the input string so it fits nicely in a JSON document.  This includes
 escaping double quote and slash characters.
 @return The escaped version of the input string
 */
QString LocalyticsSession::escapeString(const QString input)
{
	QString output = input;
	output.replace(QLatin1String("\\"), QLatin1String("\\\\"))
          .replace(QLatin1String("\""), QLatin1String("\\\""))
          .replace(QLatin1String("\n"), QLatin1String("\\n"))
          .replace(QLatin1String("\t"), QLatin1String("\\t"))
          .replace(QLatin1String("\b"), QLatin1String("\\b"))
          .replace(QLatin1String("\r"), QLatin1String("\\r"))
          .replace(QLatin1String("\f"), QLatin1String("\\f"));
   return output;
}


void LocalyticsSession::logMessage(QString msg)
{
  if (DO_LOCALYTICS_LOGGING)
    {
      qDebug() << "(localytics) " <<  msg;
    }
}


/*!
  @method customDimensions
  @abstract Returns the json blob containing the custom dimensions. Assumes this will be appended
  to an existing blob and as a result prepends the results with a comma.
*/
QString LocalyticsSession::customDimensions()
{
  QString dimensions;
  
  for(int i=0; i <4; i++)
    {
      QString dimension = LocalyticsDatabase::sharedLocalyticsDatabase()->customDimension(i);
      if (!dimension.isEmpty())
        {
          dimensions.append(QString(QLatin1String(",\"c%1\":\"%2\"")).arg(i).arg(dimension));
        }
    }
  return dimensions;
}

/*!
  @method locationDimensions
  @abstract Returns the json blob containing the current location if available or nil if no location is available.
*/
QString LocalyticsSession::locationDimensions()
{
  return QString();
  //  if (lastDeviceLocation.latitude == 0 || lastDeviceLocation.longitude == 0)
  //  {
  //    return QString("");
  //  }

  //return [NSString stringWithFormat:@",\"lat\":%f,\"lng\":%f",
  //        lastDeviceLocation.latitude,
  //        lastDeviceLocation.longitude];
}

/*!
  @method hashString
  @abstract SHA1 Hashes a string
*/
QString LocalyticsSession::hashString(QString input)
{
  QByteArray stringBytes = input.toUtf8();
  QByteArray digest = QCryptographicHash::hash(stringBytes, QCryptographicHash::Sha1);
  return QString::fromAscii(digest.toHex());
}

/*!
  @method randomUUID
  @abstract Generates a random UUID
  @return QString containing the new UUID
*/
QString LocalyticsSession::randomUUID()
{
  QUuid uu = QUuid::createUuid();
  return uu.toString();
}

/*!
 @method installationId
 @abstract Looks in user preferences for an ID unique to this installation. If one is not
 found it checks if one happens to be in the database (carroyover from older version of the db)
 if not, it generates one.
 @return A string uniquely identifying this installation of this app
 */
QString LocalyticsSession::installationId()
{
  QSettings settings;
  QString installId = settings.value(PREFERENCES_KEY).toString();

  // If it hasn't been found yet, generate a new one.
  if (installId.isEmpty())
    {
      logMessage(QLatin1String("Install ID not find one in database, generating a new one."));
      installId = randomUUID();
      // Store the newly generated installId
      settings.setValue(PREFERENCES_KEY, installId);
      settings.sync();
    }

  return installId;
}


/*!
  @method uniqueDeviceIdentifier
  @abstract A unique device identifier is a hash value composed from various hardware identifiers such
  as the device’s serial number. It is guaranteed to be unique for every device but cannot 
  be tied to a user account.
  @return An 1-way hashed identifier unique to this device.
*/
QString LocalyticsSession::uniqueDeviceIdentifier()
{
#ifdef __QNXNTO__
  bb::device::HardwareInfo hwid;
  return hashString(hwid.serialNumber());
#else
  QSettings settings;
  QString deviceId = settings.value(QLatin1String("DEVICE_ID")).toString();
  if (deviceId.isEmpty())
  {
	  deviceId = randomUUID();
	  settings.setValue(QLatin1String("DEVICE_ID"), deviceId);
	  settings.sync();
  }
  return hashString(deviceId);
#endif
}

/*!
  @method appVersion
  @abstract Gets the pretty string for this application's version.
  @return The application's version as a pretty string
*/
QString LocalyticsSession::appVersion()
{
  return QCoreApplication::applicationVersion();
  //ApplicationInfo.version()
}

qint64 LocalyticsSession::availableMemory()
{
#ifdef __QNXNTO__
  bb::MemoryInfo memoryInfo;
  return memoryInfo.availableDeviceMemory();
#else
  return 0;
#endif
}

QString LocalyticsSession::systemVersion()
{
#ifdef __QNXNTO__
	return QLatin1String("10.0");
#else
	return QLatin1String("unknown");
#endif
}

/*!
  @method isDeviceJailbroken
  @abstract checks for the existance of apt to determine whether the user is running any
  of the jailbroken app sources.
  @return whether or not the device is jailbroken.
 */
bool LocalyticsSession::isDeviceJailbroken()
{
  return false;
}

QString LocalyticsSession::deviceModel()
{
#ifdef __QNXNTO__
  bb::device::HardwareInfo info;
  qDebug() << info.modelName(); //"BlackBerry 10 Dev Alpha"
  qDebug() << info.modelNumber(); //"STL100-1"
  qDebug() << info.hardwareId(); //"0x04002607"
  return info.modelName();
#else
  return QLatin1String("unknown");
#endif
}

QString LocalyticsSession::getNetworkType()
{
#ifdef __QNXNTO__
  //TODO wifi? QLatin1String("802_11")
  bb::device::CellularRadioInfo cellular;
  bb::device::CellularTechnology::Types types = cellular.activeTechnologies();
  if (types & bb::device::CellularTechnology::Lte)  return QLatin1String("lte");
  if (types & bb::device::CellularTechnology::Evdo) return QLatin1String("evdo");
  if (types & bb::device::CellularTechnology::Cdma1X) return QLatin1String("cdma");
  if (types & bb::device::CellularTechnology::Umts) return QLatin1String("umts");
  if (types & bb::device::CellularTechnology::Gsm) return QLatin1String("gsm");
#endif
  return QLatin1String("");
}
