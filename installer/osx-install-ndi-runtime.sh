#!/bin/bash
set -ex
cd /tmp
rm -f ndiruntime.pkg
curl -L -o ndiruntime.pkg http://new.tk/NDIRedistV4Apple
sudo installer -allowUntrusted -package ./ndiruntime.pkg -target /
