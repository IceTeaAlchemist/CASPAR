#!/usr/bin/env bash
# To run
# ./runSetup.sh
# 20220711 weg, started with Nick.

# Function that echoes the command to the terminal.
domod () {
        echo "$@"
        $@
}

# Check if node (with npm) were installed.  Is in the systemSetup.sh also.
which node
if [ $? != 0 ]; then
    # These "sudo"s below did not quit do it once.  Added -E, stop if any command fails.
    domod sudo curl -fsSL https://deb.nodesource.com/setup_16.x | sudo -E bash -
    domod sudo apt-get install -y nodejs
    echo
    echo "node version `node -v`"
    echo
fi

echo "npm init"
npm init<<EOF
caspar
0.0.1


ISC
yes
EOF
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

# Fix that the script is being run as sudo for the build dir.
domod sudo chown -R pi.pi build

domod mkdir data
domod mkdir configs
domod touch ./configs/configs.txt

domod sudo cp -i ./UI/CASPAR-UI.html /var/www/html/

echo "sudo cp -fpr ./\"JS Libraries/src\" /var/www/html/"
sudo cp -fpr ./"JS Libraries/src" /var/www/html/
