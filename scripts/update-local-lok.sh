#!/bin/sh

echo "Ensure you run this from the root of the repo"

LOK_PATH=$1

if [ -z "$LOK_PATH" ]
then
  echo "LOK_PATH not set. Defaulting to ../libreofficekit"

  LOK_PATH="../libreofficekit"
fi

echo "Removing existing libreofficekit"

rm -rf ./src/out/Default/libreofficekit
mkdir ./src/out/Default/libreofficekit

echo "Bringing over local libreofficekit"

cp -rf $LOK_PATH/libreoffice-core/instdir/* ./src/out/Default/libreofficekit/

echo "Complete"
