#!/bin/sh
cp ./openhab /etc/init.d/openhab
chmod a+x /etc/init.d/openhab
#update-rc.d /etc/init.d/openhab defaults
sed -i -e '$i /etc/init.d/openhab start &\n' /etc/rc.local
