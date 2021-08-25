#!/bin/bash

#fw_file=$1
fw_file='tasmota-wifiman.bin'

printf "tasmota FW file: $fw_file\n"

set -o errexit

printf "\nInstalling fw on the server\n"
cp $fw_file /var/www/html/

printf "\nGet deviceIP and deviceID\n"
device_id=$(avahi-browse -t _ewelink._tcp --resolve -p)
echo $device_id
device_ip=$device_id

device_id="${device_id/*id=/}" # replaces ...id= with empty string
device_id="${device_id/'"'*/}" # replaces "...= with empty string
echo "Device ID: $device_id"

IFS=';'
array=( $device_ip )
device_ip=${array[12]}
echo "Device IP: $device_ip"

#Get info
printf "\nGet device info\n"
device_info=$(curl http://$device_ip:8081/zeroconf/info -XPOST --data '{"deviceid":"$device_id","data":{} }')
echo $device_info

#Unlock
printf "\nUnlock device\n"
device_unlock=$(curl http://$device_ip:8081/zeroconf/ota_unlock -XPOST --data '{"deviceid":"$device_id","data":{} }')
echo $device_unlock

#Calculate sha256sum
printf "\n#Calculate sha256sum\n"
shasum=$(shasum -a 256 /var/www/html/$fw_file)
IFS=' '
array=( $shasum )
shasum=${array[0]}
printf "\nSHAsum: $shasum\n"

#FW downloadUrl
printf "\nDownload FW\n"
fw_dowanlod="curl http://$device_ip:8081/zeroconf/ota_flash -XPOST --data '{\"deviceid\":\"$device_id\",\"data\":{\"downloadUrl\": \"http://sonoffdiy/$fw_file\", \"sha256sum\": \"$shasum\"} }'"
echo $fw_dowanlod
#fw_dowanlod=$(curl http://$device_ip:8081/zeroconf/ota_flash -XPOST --data '{"deviceid":"$device_id","data":{"downloadUrl": "http://sonoffdiy/$fw_file", "sha256sum": "$shasum"} }')
fw_dowanlod=$(eval $fw_dowanlod)
echo $fw_dowanlod



