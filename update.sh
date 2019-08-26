#!/bin/bash

git add .

git commit -m "commit"

git pull origin master

git push -u origin master
sleep 3
echo "3522163248@qq.com" |sudo -S apt-get update
echo "djy123djy" | sudo -S apt-get update

