---
layout: default
---

# Remote control for iOS devices from a Linux desktop
Aka [scrcpy](https://github.com/Genymobile/scrcpy) but for [iOS](https://i.imgur.com/0yTw52P.jpg) and from Linux.  

Since ["Apple is the only platform compatible with everything"](https://www.urbandictionary.com/define.php?term=Clown%20World) controlling it from [linux](https://github.com/OverMighty/i-use-arch-btw) can be [quite a challenge](https://youtu.be/fPNdWnwuBDI?t=10).
Thanks to some [new](https://ell.stackexchange.com/questions/30805/is-it-correct-to-say-a-program-is-buggy) projects, it is now possible. By combining [xdotool](https://pypi.org/project/python-libxdo/), [UxPlay](https://github.com/antimof/UxPlay) and [zxtouch](https://github.com/xuan32546/IOS13-SimulateTouch) you can view and control your [Jailbroken](https://stallman.org/saintignucius.jpg) [iOS device](https://en.wikipedia.org/wiki/Scam_(disambiguation)) from a Linux desktop over the [network](https://en.wikipedia.org/wiki/Series_of_tubes).

## zxtouch
This is responsible for simulating [touch](https://www.youtube.com/watch?v=cUqCv_1kGzM) events on the device based on network calls.
You can install this from cydia by adding the repo:  
[https://zxtouch.net](https://zxtouch.net)  
And then installing the zxtouch [app](https://news.artnet.com/app/news-upload/2015/10/south-park-yaoi-4.jpg)  
It will start listening on port :6000 after installation.

## UxPlay
This allows you to see your [device's screen](https://www.theonion.com/report-90-of-waking-hours-spent-staring-at-glowing-re-1819570829) on your linux desktop via apple's screen mirroring.
Install [UxPlay](https://github.com/antimof/UxPlay) on your desktop.
Try running it, it will throw some avahi warnings, [just ignore that](https://youtu.be/DstWZY_eUOc?t=74).
Your phone will likely not see it right away or will not be able to connect to it.
When this happens run:
```
sudo systemctl stop systemd-resolved.service
sudo systemctl restart avahi-daemon.service
```
honestly not sure if the first one is necessary.

## scrgto
Once both of these are installed, here's a script to connect them.
You want the clicks on your UxPlay mirrored screen to be translated into API calls for zxtouch.
Enter [scrgto](https://github.com/f-viktor/ghetto-scrcpy-iOS)
It uses xdotool to register your clicks on the window and send them to the iOS device.
To use it, install python-libxdo. And then just run the script.
It will first ask you to click on the window where your mirrored screen is, then if the connection is successful, any subsequent clicks are sent to the [phone](https://www.youtube.com/watch?v=IGL_gBXhhCU).
