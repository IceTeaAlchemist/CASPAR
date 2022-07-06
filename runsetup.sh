#!/usr/bin/env bash

npm init
npm install ws
npm install nodemailer
npm install fs
npm install http

# sudo npm install -g node-gyp
node-gyp configure
node-gyp build

mkdir data
mkdir configurations
touch ./configurations/configs.txt

sudo cp -i ./UI/CASPAR-UI.html /var/www/html/


