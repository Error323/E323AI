#!/bin/sh

git pull
CURDIR=`pwd`

# linux compile
cd ~/workspace/spring/linux
rm -rf AI/Skirmish/E323AI
make -j5 E323AI

# windows compile
cd ~/workspace/spring/win
rm -rf AI/Skirmish/E323AI
make -j5 E323AI

cd ${CURDIR}

VERSION=`cat ../VERSION`

DIR=AI/Skirmish/E323AI/${VERSION}

mkdir -vp ${DIR}

cp -vr ../data/* ${DIR}
cp -vr ../../../../linux/AI/Skirmish/E323AI/libSkirmishAI.so ${DIR}
cp -vr ../../../../win/AI/Skirmish/E323AI/SkirmishAI.dll ${DIR}

tar -cvzf E323AI-v${VERSION}-HighTemplar.tgz AI

rm -rfv AI
