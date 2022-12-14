#!/bin/sh
echo "Ensure this is run from the root of the repo"

LOK_PATH=$1

BASE_PATH=$(pwd)
ELECTRON_OUT_DIR=Default
DEFAULT_LOK_PATH=../libreofficekit

if [ -z "$LOK_PATH" ]
then
  echo "LOK_PATH not set. Defaulting to $DEFAULT_LOK_PATH"
  LOK_PATH=$DEFAULT_LOK_PATH
else
  echo "LOK_PATH was set to $LOK_PATH"
fi

OUT_DIR="$BASE_PATH/src/out/$ELECTRON_OUT_DIR"

echo "Removing existing libreofficekit"

rm -rf $OUT_DIR/libreofficekit
mkdir $OUT_DIR/libreofficekit

echo "Bringing over local libreofficekit"

cp -rf $LOK_PATH/libreoffice-core/instdir/* $OUT_DIR/libreofficekit/

echo "Complete"
