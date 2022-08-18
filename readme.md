# CASPAR

Have tried to make the setup work from scripts:<br>
<br>
<b>systemSetup.sh</b> should be run on a new SD Drive Raspberry Pi drive.  Should install nodejs, node-gyp, and WiringPi from Nick's github, https://github.com/IceTeaAlchemist/WiringPi .  Likely run once for a new SD Drive / Raspberry Pi configuration.<br>
```cd ${HOME}/Documents/githubs
git clone https://github.com/IceTeaAlchemist/CASPAR
cd CASPAR
sudo bash ./systemSetup.sh
```
<br>
<br>
<b>runSetup.sh</b> should be run after the cloning of CASPAR<br>
```cd ${HOME}/Documents/githubs
git clone https://github.com/IceTeaAlchemist/CASPAR
cd CASPAR
sudo bash ./runSetup.sh
node-gyp rebuild    ( or for partial rebuild node-gyp build )
```

==================================<br>
This is some direction for setup.<br>

First get an Access Token for "git clone" the repo.<br>

Check node is installed, "node -v" and use version 16.<br>
https://github.com/nodesource/distributions <br>
sudo curl -fsSL https://deb.nodesource.com/setup_16.x | sudo bash - <br>
sudo apt-get install -y nodejs <br>
node -v , is now v16.15.1  and we are FIXED on that for now.<br>

<br>
sudo npm install node-gyp<br>

in /githubs/CASPAR:<br>

npm init<br>
npm install every package in 'require' except casparengine<br>
    npm install ws<br>
    npm install nodemailer<br>
    npm install fs<br>
    npm install -g node-gyp<br>
<br>
node-gyp configure
<br>
<br>
node-gyp build<br>
node caspar.js (be sure to create ./data/ directory by hand)<br>
<br>

After updating software, maybe restarting the RPi, we found we needed to restart hostapd and dnsmasq to restart the wifi as access point:<br>
sudo systemctl restart dnsmasq<br>
sudo systemctl restart hostapd<br>
<br>

This contains the current, weekly updated version of the CASPAR device code until someone makes me change the name.
