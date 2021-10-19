---
layout: default
---

# Basic setup for iOS testing

This page is a reminder about tools, processes, sites handy for testing iOS applications from linux.  
Given that "Mac is the only platform that is compatible with everything", it is not compatible with linux, and so a lot of janky messing around is required.

# Jailbreak

Check what sort of jaiblreak you need from this page.  
[https://www.theiphonewiki.com/wiki/Jailbreak](https://www.theiphonewiki.com/wiki/Jailbreak)  
[Checkra1n](https://checkra.in/) has [Linux support](https://aur.archlinux.org/packages/checkra1n-cli/) for sure.  
You will need to jailbreak your phone if you plan on testing anything.  
Install cydia too, Checkra1n has a simple button for this.  
Do not wipe the device from the general settings after this, it will break cydia.  


# Installing tools on device

### SSH
Install the [OpenSSH](https://cydia.saurik.com/package/openssh/) app from cydia.  
It will instantly start listening on :22  
user: root  
pw: alpine  

### Terminal emulator
Install [NewTerm 2](https://github.com/hbang/NewTerm/releases) for a term emulator  
`su root` for root access

### Frida-server
Add the [Frida](https://frida.re/docs/quickstart/) repository:  
[https://build.frida.re](https://build.frida.re)  
Install the relevant Frida app for your device  
If it doesn't start automatically you may try to start it from the root folder as `./frida-server`  


### Installing burp certificate

[PortSwigger's article](https://portswigger.net/support/installing-burp-suites-ca-certificate-in-an-ios-device)  

1. Download the certificate and install it
2. Settings > profiles or something > install
3. Settings > General > About > Certificate Trust Settings > enable it

# Using tools on Linux

### python-objection
`objection -g "com.app.name" explore`  
`ios keychain dump`  
`ios info binary`  
`ios bundles list_bundles`  
`ios cookies get`  
`ios nsurlcredentialstorage dump`  


### frida-ps
List processes on the device `frida-ps -U`

### frida-trace
`frida-trace -U -i "*rtmp*" Appname`   
`frida-trace -i "write*" rsyslogd`

### app locations
App data is usually stored under   
`/private/var/mobile/Containers/Data/Application/<budle-id>`  
you can just `find | grep appname`  


### UxPlay
Install [UxPlay](https://github.com/antimof/UxPlay) for screen mirroring   
Check out [this article](https://f-viktor.github.io/articles/scrgto) on how to control your iOS device from your  Linux desktop.  
