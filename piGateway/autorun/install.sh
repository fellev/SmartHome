#!/bin/sh
cp pigateway /etc/init.d/pigateway
chmod 777 /etc/init.d/pigateway
update-rc.d pigateway defaults
