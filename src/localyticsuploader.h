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

#ifndef LOCALYTICSUPLOADER_H
#define LOCALYTICSUPLOADER_H

#include <QtCore/QObject>
#include <QtNetwork/QNetworkReply>

class QNetworkAccessManager;

class LocalyticsUploader : public QObject
{
  Q_OBJECT
  public:
  /*!
    Establishes this as a Singleton Class allowing for data persistence.
  */
  static LocalyticsUploader* sharedLocalyticsUploader()
  {
    if (!_sharedLocalyticsUploader)
      _sharedLocalyticsUploader = new LocalyticsUploader;
    return _sharedLocalyticsUploader;
  }

  /*!  Determine if the Uploader is uploading data right now.

    \return `true` if the uploader is in the process of uploading,
    `false` otherwise.
  */
  bool isUploading()
  {
    return _isUploading;
  }

  /*!
    Creates a thread which uploads all queued header and event data.
    All files starting with `sessionFilePrefix` are renamed, uploaded
    and deleted on upload.  This way the sessions can continue writing
    data regardless of whether or not the upload succeeds.  Files
    which have been renamed still count towards the total number of
    Localytics files which can be stored on the disk.
    
    This version of the method now just calls the second version of it with a nil target and NULL callback method.

    \param localyticsApplicationKey the Localytics application ID
    \param httpsMode Flag determining whether HTTP or HTTPS is used
    for the POST URL.
    \param installId Install id passed to the server in the
    x-install-id header field.
  */
  void upload(QString localyticsApplicationKey, bool httpsMode, QString installId);


signals:
  /*!
    Emitted when the upload is complete.
    
    There is no notification of success or failure.
  */
  void uploadComplete();

private slots:
  void replyFinished(QNetworkReply*);
    
private:
  explicit LocalyticsUploader(QObject *parent = 0);
  void logMessage(QString message);
  QByteArray gzipDeflate(QByteArray data);
  QString uploadTimestamp();
  void finishUpload();
  QNetworkAccessManager *m_networkManager;
  bool _isUploading;
  static LocalyticsUploader* _sharedLocalyticsUploader;
};

#endif // LOCALYTICSUPLOADER_H
