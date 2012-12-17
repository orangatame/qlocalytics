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

#ifndef WEBSERVICECONSTANTS_H
#define WEBSERVICECONSTANTS_H

// The constants which are used to make up the JSON blob
// To save disk space and network bandwidth all the keywords have been
// abbreviated and are exploded by the server.

/*****************
 * Upload Header *
 *****************/
#define HEADER_CLIENT_TIME QLatin1String("x-upload-time")
#define HEADER_INSTALL_ID  QLatin1String("x-install-id")

/*********************
 * Shared Attributes *
 *********************/
#define PARAM_UUID                  QLatin1String("u")        // UUID for JSON document
#define PARAM_DATA_TYPE             QLatin1String("dt")       // Data Type
#define PARAM_CLIENT_TIME           QLatin1String("ct")       // Client Time, seconds from Unix epoch (int)
#define PARAM_LATITUDE              QLatin1String("lat")      // Latitude - if available
#define PARAM_LONGITUDE             QLatin1String("lon")      // Longitude - if available
#define PARAM_SESSION_UUID          QLatin1String("su")       // UUID for an existing session
#define PARAM_NEW_SESSION_UUID      QLatin1String("u")        // UUID for a new session
#define PARAM_ATTRIBUTES            QLatin1String("attrs")    // Attributes (dictionary)
#define PARAM_SESSION_ELAPSE_TIME   QLatin1String("sl")       // Number of seconds since the previous session start

/***************
 * Blob Header *
 ***************/

// PARAM_UUID
// PARAM_DATA_TYPE => "h" for Header
// PARAM_ATTRIBUTES => dictionary containing Header Common Attributes
#define PARAM_PERSISTED_AT          QLatin1String("pa")       // Persistent Storage Created At. A timestamp created when the app was
                                                              // first launched and the persistent storage was created. Stores as
                                                              // seconds from Unix epoch. (int)
#define PARAM_SEQUENCE_NUMBER       QLatin1String("seq")      // Sequence number - an increasing count for each blob, stored in the
                                                              // persistent store Consistent across app starts. (int)

/****************************
 * Header Common Attributes *
 ****************************/

// PARAM_DATA_TYPE
#define PARAM_APP_KEY               QLatin1String("au")       // Localytics Application ID
#define PARAM_DEVICE_UUID           QLatin1String("du")       // Device UUID (deprecated)
#define PARAM_DEVICE_UUID_HASHED    QLatin1String("udid")     // Hashed version of the UUID
#define PARAM_DEVICE_ADID           QLatin1String("adid")     // Advertising Identifier
#define PARAM_INSTALL_ID            QLatin1String("iu")       // Install ID
#define PARAM_JAILBROKEN            QLatin1String("j")        // Jailbroken (boolean)
#define PARAM_LIBRARY_VERSION       QLatin1String("lv")       // Client (localytics library) Version
#define PARAM_APP_VERSION           QLatin1String("av")       // Application Version
#define PARAM_DEVICE_PLATFORM       QLatin1String("dp")       // Device Platform
#define PARAM_DEVICE_MANUFACTURER   QLatin1String("dma")      // Device Manufacturer (optional)
#define PARAM_LOCALE_LANGUAGE       QLatin1String("dll")      // Device Language (optional)
#define PARAM_LOCALE_COUNTRY        QLatin1String("dlc")      // Locale Country (optional)
#define PARAM_DEVICE_COUNTRY        QLatin1String("dc")       // Device Country (iso code)
#define PARAM_DEVICE_MODEL          QLatin1String("dmo")      // Device Model
#define PARAM_DEVICE_OS_VERSION     QLatin1String("dov")      // Device OS Version
#define PARAM_DATA_CONNECTION_TYPE  QLatin1String("dac")      // Data Connection Type (optional)
#define PARAM_NETWORK_CARRIER       QLatin1String("nca")      // Network Carrier
#define PARAM_OPT_VALUE             QLatin1String("out")      // Opt Out (boolean)
#define PARAM_DEVICE_MEMORY         QLatin1String("dmem")     // Device Memory

/*****************
 * Session Start *
 *****************/

// PARAM_UUID
// PARAM_DATA_TYPE => "s" for Start
// PARAM_CLIENT_TIME
#define PARAM_SESSION_NUMBER        QLatin1String("nth")      // This is the nth session on the device, 1-indexed (int)

/****************
 * Session Stop *
 ****************/

// PARAM_UUID
// PARAM_DATA_TYPE => "c" for Close
// PARAM_CLIENT_TIME
// PARAM_LATITUDE
// PARAM_LONGITUDE
// PARAM_SESSION_UUID => UUID of session being closed
#define PARAM_SESSION_ACTIVE        QLatin1String("cta")      // Active time in seconds (time app was active)
#define PARAM_SESSION_TOTAL         QLatin1String("ctl")      // Total session length
#define PARAM_SESSION_SCREENFLOW    QLatin1String("fl")       // Screens encountered during this session, in order

/*********************
 * Application Event *
 *********************/

// PARAM_UUID
// PARAM_DATA_TYPE => "e" for Event
// PARAM_CLIENT_TIME
// PARAM_LATITUDE
// PARAM_LONGITUDE
// PARAM_SESSION_UUID => UUID of session event occured in
// PARAM_ATTRIBUTES => dictionary containing attributes for this event as key-value string pairs
#define PARAM_EVENT_NAME            QLatin1String("n")        // Event Name, (eg. 'Button Click')
#define PARAM_REPORT_ATTRIBUTES     QLatin1String("rattrs")   // Attributes used in custom reports

/********************
 * Application flow *
 ********************/

// PARAM_UUID
// PARAM_DATA_TYPE => "f" for Flow
// PARAM_CLIENT_TIME
#define PARAM_SESSION_START         QLatin1String("ss")   // Start time for the current session.
#define PARAM_NEW_FLOW_EVENTS       QLatin1String("nw")   // Events and screens encountered during this session that have NOT been staged for upload.
#define PARAM_OLD_FLOW_EVENTS       QLatin1String("od")   // Events and screens encountered during this session that HAVE been staged for upload.

#endif // WEBSERVICECONSTANTS_H
