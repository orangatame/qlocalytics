# BlackBerry 10 port of the Localytics instrumentation classes

[Localytics](http://www.localytics.com/) does not currently provide an
instrumentation library for BlackBerry 10.  This project is a port of
[the iOS library](http://www.localytics.com/docs/iphone-integration/).

## License
See LICENSE.txt


## How to instrument your Qt app
### Setup
In yourapp.cpp:
````cpp
void YourApp::onStart()
    LocalyticsSession *s = LocalyticsSession::sharedLocalyticsSession();
    s->startSession(QLatin1String(LOCALYTICS_APP_KEY));
}

// This is analog to iOS's "app was backgrounded"
void YourApp::thumbnail()
{
    LocalyticsSession::sharedLocalyticsSession()->saveApplicationFlowAndRemoveOnResume(true);
}

// Analagous to "app was brought back to foreground"
void YourApp::fullscreen()
{
    LocalyticsSession::sharedLocalyticsSession()->resume();
}
void YourApp::aboutToQuit()
{
    LocalyticsSession::sharedLocalyticsSession()->close();
}
````
in your class that extends [bb::cascades::application](https://developer.blackberry.com/cascades/reference/bb__cascades__application.html).


Also, connect these signals in main.cpp:
````cpp
Q_DECL_EXPORT int main(int argc, char **argv)
{
    //....
    YourApp mainApp;
    QObject::connect(&app, SIGNAL(thumbnail()), &mainApp, SLOT(thumbnail()));
    QObject::connect(&app, SIGNAL(fullscreen()), &mainApp, SLOT(fullscreen()));
    QObject::connect(&app, SIGNAL(aboutToQuit()), &mainApp, SLOT(aboutToQuit()));
    mainApp.onStart();
    return Application::exec();
}
````

### Tagging events (optional)
To tag an event, use the following line of code:

You can attach attributes to an event by passing a Dictionary<string, string> as a second argument to TagEvent().  For example, to collect information about unhandled exceptions, you could use the following code:

````cpp
    QVariantMap attr;
    attr["Barcode Length"] = QString::number(barcode_truncated.length());
    LocalyticsSession::sharedLocalyticsSession()->tagEvent("Barcode Added", attr);
````cpp

## Notes
- Screen flows are not currently supported.
- I haven't found a great way to get the BlackBerry 10 OS
  version. There have only been a few betas, so this hasn't been an
  issue yet.
- The Device UDID relies on the "read_device_identifying_information"
  permission. Make sure to add it to your "bar-descriptor.xml".
- Many things are ported from the iOS version. Please pardon any
  reminants of Objective-C in these sources.

## Thanks
- Thanks to
  [localytics-win8](https://github.com/elyscape/localytics-win8) for
  documenting more of the Localytics API and keys, and providing a
  good markdown template for a README
- Thanks to [qjson](http://gitorious.org/qjson/qjson/) for providing a
  good product structure, including QTest unit tests and cmake
  examples.
- Thanks to RIM for producing BlackBerry 10!
