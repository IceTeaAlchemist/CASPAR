#!/usr/bin/env bash

# Run as sudo
# sudo systemSetup.sh
#
# Setup for new SD Drive for the Raspberry Pi 4 used in the
# CASPAR adaptive PCR machine.
# 20220817 weg, started with Nick.

# Function that echoes the command to the terminal.
domod () {
        echo "$@"
        $@
}

domod sudo apt install emacs -y

domod sudo systemctl enable ssh
domod sudo systemctl start ssh
domod sudo raspi-config nonint do_i2c 0

myhostname=rpi4
domod sudo hostnamectl set-hostname ${myhostname}

cat >> ${HOME}/.bash_aliases <<EOF
alias rm='rm -i'
alias mv='mv -i'
alias cp='cp -i'  
EOF

mkdir ${HOME}/bin
mkdir ${HOME}/Documents/github

cat >> ${HOME}/.Profile <<EOF
PATH="$PATH:/."
EOF

# Network stuff, see
# https://thepi.io/how-to-use-your-raspberry-pi-as-a-wireless-access-point/
sudo apt install -y hostapd
sudo apt install -y dnsmasq

sudo systemctl stop hostapd
sudo systemctl stop dnsmasq

# Not in tutorial, in Pi's documentation.
sudo systemctl unmask hostapd
sudo systemctl enable hostapd

# Networking
<<<<<<< HEAD
sudo cat >> /etc/dhcpcd.conf<<EOF
=======
sudo cat >> /etc/dhcpd.conf<<EOF
>>>>>>> 94d3f6a204fe98eb6671d0efed4f3d55cf062400
interface wlan0
nohook wpa_supplicant
static ip_address=192.168.0.10/24
denyinterfaces wlan0
EOF

sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.orig
sudo cat >> /etc/dnsmasq.conf <<EOF
interface=wlan0
  dhcp-range=192.168.0.11,192.168.0.30,255.255.255.0,24h
EOF

NETWORK=PRODUCTION2
PASSWORD=thefriendlyghostinthemachine

# After install of hostapd.
sudo cat >> /etc/hostapd/hostapd.conf <<EOF
interface=wlan0
<<<<<<< HEAD
#bridge=br0
=======
bridge=br0
>>>>>>> 94d3f6a204fe98eb6671d0efed4f3d55cf062400
hw_mode=g
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
ssid=${NETWORK}
wpa_passphrase=${PASSWORD}
EOF

sudo cat >> /etc/default/hostapd <<EOF
DAEMON_CONF="/etc/hostapd/hostapd.conf"
EOF
# End Tutorial at the Step 6, do not do it.

# Not in the tutorial, think it fixes the race condition with network start.
sudo mkdir /etc/systemd/system/hostapd.service.d 
sudo cat >> /etc/systemd/system/hostapd.service.d/override.conf <<EOF
[Unit]
After=network-online.target
Wants=network-online.target
EOF

sudo mkdir /etc/systemd/system/dnsmasq.service.d 
sudo cat >> /etc/systemd/system/dnsmasq.service.d/override.conf <<EOF
[Unit]
After=network-online.target
Wants=network-online.target
EOF

# Check if node (with npm) were installed.
# Check node is installed, "node -v" and use version 16.
# https://github.com/nodesource/distributions
which node
if [ $? != 0 ]; then
    # These "sudo"s below did not quit do it once.  Added -E, stop if any command fails.
    domod sudo curl -fsSL https://deb.nodesource.com/setup_16.x | sudo -E bash -
    domod sudo apt-get install -y nodejs
    #node -v , is now v16.15.1 and we are FIXED on that for now.

    #else
    #nodeVers=`node -v`
    #if nodeVers is not v16.14.0 with the v16 install v16??

    echo
    echo "node version `node -v`"
    echo
fi

# Once created version v12.22.12 for whatever reason so check that and try
# again(?).  weg 20220818
domod hash -r
myversion=`node -v`

# Test that node-gyp is installed.  Which returns 0 if it finds it, 1 if not.
which node-gyp
if [ $? != 0 ]; then
    domod sudo npm install -g node-gyp
fi

# WiringPi install.
# From github https://github.com/IceTeaAlchemist/WiringPi .

WPiGithub="https://github.com/IceTeaAlchemist/WiringPi"
WPiDir="${HOME}/Documents/githubs/WiringPi/"

cat > /tmp/testWPi.cpp<<EOF
#include <wiringPi.h>
int main(void){
  int idum=321;
  return 0;
}// end main
EOF
g++ -c /tmp/testWPi.cpp
if [ $? != 0 ]; then
    mkdir -p ${HOME}/Documents/githubs
    cd ${HOME}/Documents/githubs
    git clone $WPiGithub
    cd ${HOME}/Documents/githubs/WiringPi
    sudo ./build
fi

<<<<<<< HEAD
# Install the apache2 httpd server.  Should turn on and enabel by default.
domod sudo apt-get install -y apache2

=======
>>>>>>> 94d3f6a204fe98eb6671d0efed4f3d55cf062400
echo
echo "Reboot the system."
echo "Hope you have ethernet or other wlan for connection."


<<<<<<< HEAD
# Optional, Install VS Code,
# sudo apt install code
# and should install.
#
=======
>>>>>>> 94d3f6a204fe98eb6671d0efed4f3d55cf062400
