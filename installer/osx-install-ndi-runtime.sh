#!/bin/bash
set -ex
cd /tmp
rm ndiruntime.pkg
curl -L -o ndiruntime.pkg http://new.tk/NDIRedistV3Apple
sudo installer -allowUntrusted -package ./ndiruntime.pkg -target /
