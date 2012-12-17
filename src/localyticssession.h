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

/* -*- mode:c++-mode -*- */
#ifndef LOCALYTICSSESSION_H
#define LOCALYTICSSESSION_H


#include <QObject>
#include <QDateTime>
#include <QVariantMap>

// Set this to true to enable localytics traces (useful for debugging)
#define DO_LOCALYTICS_LOGGING true

class LocalyticsSession : public QObject
{
    Q_OBJECT
public:
    explicit LocalyticsSession(QObject *parent = 0);
    friend class SessionTest;
    
    static LocalyticsSession* sharedLocalyticsSession() {
        if (!_sharedLocalyticsSession) {
            _sharedLocalyticsSession = new LocalyticsSession;
        }
        return _sharedLocalyticsSession;
    }
  /*!
    Initializes the Localytics Object. Not necessary if you choose to
    use startSession().
    
    \param appKey The key unique for each application
    generated at http://www.localytics.com
  */
  void init(QString appKey);

  /*!  
    An optional convenience initialize method that also calls the
    init(), open() and upload() methods.

    Best Practice is to call open & upload immediately after
    Localytics Session when loading an app, this method facilitates
    that behavior.

    It is recommended that this call be placed in
    `applicationDidFinishLaunching`.

    \param appKey The key unique for each application
    generated at http://www.localytics.com
  */
  void startSession(QString appKey);
  
  /*!  
    (OPTIONAL) Allows the application to control whether or not it
    will collect user data.

    Even if this call is used, it is necessary to continue calling
    upload().  No new data will be collected, so nothing new will be
    uploaded but it is necessary to upload an event telling the server
    this user has opted out.
    
    \param optedIn `true` if the user is opted in, `false` otherwise.
  */
  void setOptIn(bool optedIn);
  
  /*!
    (OPTIONAL) Whether or not this user has is opted in or out.

    The only way they can be opted out is if `setOptIn(false)` has
    been called before this.  This function should only be used to
    pre-populate a checkbox in an options menu.  It is not recommended
    that an application branch based on Localytics instrumentation
    because this creates an additional test case.  If the app is opted
    out, all subsequent Localytics calls will return immediately.

    \result `true` if the user is opted in, `false` otherwise.
  */
  bool isOptedIn();
  
  /*!
    Opens the Localytics session. Not necessary if you choose to
    use startSession().
    
    The session time as presented on the website is the time between
    `open` and the final `close` so it is recommended to open the
    session as early as possible, and close it at the last moment.
    The session must be opened before any tags can be written.  It is
    recommended that this call be placed in
    `applicationDidFinishLaunching`.

    If for any reason this is called more than once every subsequent
    open call will be ignored.
    
    \sa close()
  */
  void open();
  
  /*!
    Resumes the Localytics session. 

    When the App enters the background, the session is closed and the
    time of closing is recorded.  When the app returns to the
    foreground, the session is resumed.  If the time since closing is
    greater than `BACKGROUND_SESSION_TIMEOUT`, (15 seconds by default)
    a new session is created, and uploading is triggered.  Otherwise,
    the previous session is reopened.
    
    \return Returns whether the session was resumed (`true`) or a new session
    was started (`false`). If the user has opted out of analytics then the
    return from this method is undefined.
  */
  bool resume();

  /*!
    Closes the Localytics session.

    This should be called in `applicationWillTerminate`.
    
    If close is not called, the session will still be uploaded but no
    events will be processed and the session time will not appear. This is
    because the session is not yet closed so it should not be used in
    comparison with sessions which are closed.
  */
  void close();

  /*!
    Creates a low priority thread which uploads any Localytics data already stored 
    on the device.

    This should be done early in the process life in order to
    guarantee as much time as possible for slow connections to
    complete.  It is also reasonable to upload again when the
    application is exiting because if the upload is cancelled the data
    will just get uploaded the next time the app comes up.
  */
  void upload();

  /*!
   Allows a session to tag a particular event as having occurred.

   For example, if a view has three buttons, it might make sense to
   tag each button click with the name of the button which was
   clicked.  For another example, in a game with many levels it might
   be valuable to create a new tag every time the user gets to a new
   level in order to determine how far the average user is progressing
   in the game.

   <strong>Tagging Best Practices</strong>
   <ul>
   <li>DO NOT use tags to record personally identifiable information.</li>
   <li>The best way to use tags is to create all the tag strings as predefined
   constants and only use those.  This is more efficient and removes the risk of
   collecting personal information.</li>
   <li>Do not set tags inside loops or any other place which gets called
   frequently.  This can cause a lot of data to be stored and uploaded.</li>
   </ul>
   <br>
   See the tagging guide at: http://wiki.localytics.com/
   \param event The name of the event which occurred.
   */
  void tagEvent(const QString &event);
  void tagEvent(const QString &event, const QVariantMap &attributes);
  void tagEvent(const QString &event, const QVariantMap &attributes, const QVariantMap &reportAttributes);

  bool hasInitialized() {
    return _hasInitialized;
  }
  bool saveApplicationFlowAndRemoveOnResume(bool removeOnResume);

private:
  void logMessage(QString msg);

  /* Private methods. */
  void ll_open();
  void reopenPreviousSession();
  void addFlowEvent(const QString &name, const QString& eventType);
  //void addScreenWithName(QString);
  QString blobHeaderStringWithSequenceNumber(int nextSequenceNumber);
  bool ll_isOptedIn();

// Datapoint methods.
  QString customDimensions();
  QString locationDimensions();
  QString hashString(QString input);
  QString randomUUID();
  QString escapeString(QString input);
  QString installationId();
  QString libraryVersion(); /*! Localytics lib version */
  void dequeueCloseEventBlobString();
  QString formatAttribute(QString name, QString value, bool firstAttribute);
  QString formatAttribute(QString name, QString value);
  bool createOptEvent(bool optState);

  /* Device Information */
  QString appVersion();
  QString systemVersion();
  QString deviceModel();
  QString modelSizeString();
  qint64 availableMemory();
  QString uniqueDeviceIdentifier();
  QString getNetworkType();
  bool isDeviceJailbroken(); /* from iOS */

  bool _hasInitialized;
  bool _isSessionOpen;
  float _backgroundSessionTimeout;
  bool _enableHTTPS;
  QString _sessionUUID;
  QString _applicationKey;
  QDateTime _lastSessionStartTimestamp;
  QDateTime _sessionResumeTime;
  QDateTime _sessionCloseTime;
  QString _unstagedFlowEvents;
  QString _stagedFlowEvents;
  QString _screens;
  qint64 _sessionActiveDuration; // seconds
  bool _sessionHasBeenOpen;
  quint32 _sessionNumber;
  static LocalyticsSession *_sharedLocalyticsSession;

};

#endif /* LOCALYTICSSESSION_H */
