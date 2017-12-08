 MonitorDimmer
==============

### Basics 

MonitorDimmer is a small Windows command linetools that update the bightness of the monitor depending on the ambient light.
The ambiant light is measeured using a Yocto-Light-V3 or a Yocto-Light-V2 pluge on the USB port.

### Usage



````
MonitorDimmer [options]
    
options:
-r remoteAddr : Uses remote IP devices (or VirtalHub), instead of local USB.
-m min_lux    : the Lux value for the minimal brightness.
-M min_lux    : the Lux value for the maximal brightness.
-v            : verbose mode
-l            : display Lux value of Yocto-Light-V3
-h            : display help
    
Please contact support@yoctopuce.com if you need assistance.
````



### Note
* This tool is part of an article that explain in detail how : 
https://www.yoctopuce.com/EN/article/adapting-monitor-brightness-automatically



