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

#include "localyticsuploader.h"
#include "webserviceconstants.h"
#include "localyticsdatabase.h"
#include "localyticssession.h"
#include <zlib.h>
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

#ifndef LOCALYTICS_URL
#define LOCALYTICS_URL QLatin1String("http://analytics.localytics.com/api/v2/applications/%1/uploads")
#endif

#ifndef LOCALYTICS_URL_SECURED
#define LOCALYTICS_URL_SECURED QLatin1String("https://analytics.localytics.com/api/v2/applications/%1/uploads")
#endif

LocalyticsUploader* LocalyticsUploader::_sharedLocalyticsUploader = 0;

LocalyticsUploader::LocalyticsUploader(QObject *parent) :
    QObject(parent)
{
  _isUploading = false;
  m_networkManager = new QNetworkAccessManager(this);
  connect(m_networkManager, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(replyFinished(QNetworkReply*)));
}


void LocalyticsUploader::upload(QString localyticsApplicationKey, bool useHTTPS, QString installId)
{
  if (isUploading())
    {
      logMessage(QLatin1String("Upload already in progress. Aborting"));
      return;
    }


  logMessage(QLatin1String("Beginning upload process"));
  _isUploading = true;
  
  // Prepare the data for upload.  The upload could take a long time, so some effort has to be made to be sure that events
  // which get written while the upload is taking place don't get lost or duplicated.  To achieve this, the logic is:
  // 1) Append every header row blob string and and those of its associated events to the upload string.
  // 2) Deflate and upload the data.
  // 3) On success, delete all blob headers and staged events. Events added while an upload is in process are not
  //    deleted because they are not associated a header (and cannot be until the upload completes).
  
  // Step 1
  LocalyticsDatabase *db = LocalyticsDatabase::sharedLocalyticsDatabase();
  QString blobString = db->uploadBlobString();

  if (blobString.isEmpty()) 
    {
      // There is nothing outstanding to upload.
      logMessage(QLatin1String("Abandoning upload. There are no new events."));
      finishUpload();
      return;
    }

  QByteArray requestData = blobString.toUtf8();
  logMessage(QString(QLatin1String("Uploading data (length: %1)")).arg(requestData.length()));
  logMessage(blobString);
  
  // Step 2
  QByteArray deflatedRequestData = gzipDeflate(requestData);
  
  QString urlStringFormat;
  if (useHTTPS)
    {
      urlStringFormat = LOCALYTICS_URL_SECURED;
    } 
  else
    {
      urlStringFormat = LOCALYTICS_URL;
    }
  QString apiUrlString = urlStringFormat.arg(QString(QLatin1String(QUrl::toPercentEncoding(localyticsApplicationKey))));

  QNetworkRequest request(apiUrlString);
  request.setRawHeader(QString(HEADER_CLIENT_TIME).toAscii(), uploadTimestamp().toAscii());
  request.setRawHeader(QString(HEADER_INSTALL_ID).toAscii(), installId.toAscii());
  request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-gzip"));
  request.setRawHeader(QString(QLatin1String("Content-Length")).toAscii(), QString(QLatin1String("%1")).arg(deflatedRequestData.length()).toAscii());
  logMessage(QLatin1String("Posting NOW"));
  m_networkManager->post(request, deflatedRequestData);
}

void LocalyticsUploader::replyFinished(QNetworkReply *reply)
{
  logMessage(QLatin1String("Reply finished"));
  QVariant statusCodeV = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  int responseStatusCode = statusCodeV.toInt();
  if (reply->error() != QNetworkReply::NoError)
    {
      // On error, simply print the error and close the uploader.  We
      // have to assume the data was not transmited so it is not
      // deleted.  In the event that we accidently store data which
      // was succesfully uploaded, the duplicate data will be ignored
      // by the server when it is next uploaded.
      logMessage(QString(QLatin1String("Error Uploading.  Code: %1,  Description: %2")).arg(reply->error()).arg(reply->errorString()));
    }
  else 
    {
      // Step 3 
      // While response status codes in the 5xx range leave upload
      // rows intact, the default case is to delete.
      if (responseStatusCode >= 500 && responseStatusCode < 600) 
        {
          logMessage(QString(QLatin1String("Upload failed with response status code %1")).arg(responseStatusCode));
        } 
      else
        {
          // Because only one instance of the uploader can be running at
          // a time it should not be possible for new upload rows to
          // appear so there is no fear of deleting data which has not
          // yet been uploaded.
          logMessage(QString(QLatin1String("Upload completed successfully. Response code %1")).arg(responseStatusCode));
          LocalyticsDatabase::sharedLocalyticsDatabase()->deleteUploadedData();
      }
    }
  QByteArray responseData = reply->readAll();
  if (responseData.length() > 0) 
    {
      if (DO_LOCALYTICS_LOGGING) 
        {
          QString string = QString::fromUtf8(responseData);
          logMessage(QLatin1String("Response Body: "));
          logMessage(string);
        }

    }
  finishUpload();
}

void LocalyticsUploader::finishUpload()
{
  _isUploading = false;
  LocalyticsDatabase::sharedLocalyticsDatabase()->vacuumIfRequired();
  emit uploadComplete();
}

QByteArray LocalyticsUploader::gzipDeflate(QByteArray data)
{
  if (data.length() == 0)
    return data;
	
  z_stream strm;
  
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.total_out = 0;
  strm.next_in=(Bytef *)data.data();
  strm.avail_in = data.length();
  
  // Compresssion Levels:
  //   Z_NO_COMPRESSION
  //   Z_BEST_SPEED
  //   Z_BEST_COMPRESSION
  //   Z_DEFAULT_COMPRESSION
  
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, (15+16), 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return QByteArray();
  
  QByteArray compressed(16384, '\0');  // 16K chunks for expansion
  
  do {
    
    if (abs(strm.total_out) >= compressed.length())
      compressed.resize(compressed.length() + 16384);
    
    strm.next_out = (Bytef *)compressed.data() + strm.total_out;
    strm.avail_out = compressed.length() - strm.total_out;
    
    deflate(&strm, Z_FINISH);  
    
  } while (strm.avail_out == 0);
  
  deflateEnd(&strm);
  
  compressed.resize(strm.total_out);
  return compressed;
}

void LocalyticsUploader::logMessage(QString message)
{
  if (DO_LOCALYTICS_LOGGING)
    {
      qDebug() << "(localytics uploader) " << message;
    }
}

/*!
 @method uploadTimeStamp
 @abstract Gets the current time, along with local timezone, formatted as a DateTime for the webservice. 
 @return a DateTime of the current local time and timezone.
 */
QString LocalyticsUploader::uploadTimestamp()
{
  return (QString(QLatin1String("%1")).arg((double) QDateTime::currentDateTime().toTime_t(), 0, 'f', 0));
}
