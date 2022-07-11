#!/usr/bin/env bash

# Function that echos the command to the terminal.
domod () {
        echo "$@"
        $@
}


domod npm init
domod npm install ws
domod npm install nodemailer
domod npm install fs
domod npm install http

# Test that node-gyp is installed.  Which returns 0 if it finds it, 1 if not.
which node-gyp
if [ $? != 0 ]; then
    domod sudo npm install -g node-gyp
fi

# node-gyp rebuild includes clean, configure, then build.
# node-gyp configure
domod node-gyp rebuild

domod mkdir data
domod mkdir configurations
domod touch ./configurations/configs.txt

domod sudo cp -i ./UI/CASPAR-UI.html /var/www/html/

domod sudo cp -i ./JS\ Libraries/src /var/www/html/
