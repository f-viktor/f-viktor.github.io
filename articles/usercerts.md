---
layout: default
---

# The 411
Let's say you proxied your application to inspect HTTPS traffic, but all you see in your proxy is:  
```
The client failed to negotiate an SSL connection to something.com:443: Received...
```
But you are sure the application does not have SSL pinning, and you already installed your CA certificate on the device.

# But why?

This is actually caused by an okhttp/android7+ feature where the new versions of okhttp do not trust user added certificates by default. The solution to this is as follows  
(based on a true [stackoverflow](https://stackoverflow.com/questions/38770126/has-android-changed-ssl-configuration-in-api-24/38770284#38770284) ticket)  

decompile the apk using:
```
apktool d appname.apk
```
create the file `appname/res/xml/network_security_config.xml`

Write in the file:
```xml
<?xml version="1.0" encoding="utf-8"?>

<network-security-config>
    <base-config>
        <trust-anchors>
            <certificates src="system"/>
            <certificates src="user"/>
        </trust-anchors>
    </base-config>
</network-security-config>
```
modify `appname/AndroidManifest.xml`

inside the `<application  ...... >` tag add the following:
```xml
android:networkSecurityConfig="@xml/network_security_config"
```
should look something like this:
```xml
<application android:allowBackup="true" android:appComponentFactory="android.support.v4.app.CoreComponentFactory" android:debuggable="true" android:icon="@mipmap/ic_launcher" android:label="@string/app_name" android:name="com.ddnative.App" android:roundIcon="@mipmap/ic_launcher_round" android:supportsRtl="false" android:theme="@style/AppTheme" android:networkSecurityConfig="@xml/network_security_config">
```
Recompile the apk with
```
apktool b appname -o trust.apk
```
(if it fails with something weird, try adding `--use-aapt2`)

[sign the apk](https://github.com/patrickfav/uber-apk-signer/releases/tag/v1.0.0):
```
java -jar uber-apk-signer-1.0.0.jar --apks trust.apk
```
And you're done
