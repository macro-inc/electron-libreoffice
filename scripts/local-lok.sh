#!/bin/sh

echo "Removing existing libreofficekit"

rm -rf ./src/out/Default/libreofficekit
mkdir ./src/out/Default/libreofficekit

echo "Bringing over local libreofficekit"

cp -r ./../libreofficekit/libreoffice-core/instdir/* ./src/out/Default/libreofficekit/

echo "Complete"
