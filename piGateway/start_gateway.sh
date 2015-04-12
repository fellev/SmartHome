#!/bin/sh

echo "starting pi gateway..."

cd `dirname $0`

./piGatewayBase -v -s /dev/ttyUSB0 -t +
