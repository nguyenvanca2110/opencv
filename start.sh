#!/bin/bash

./kill.sh

screen -S opencv_check -d -m -h 10000 /bin/bash -c "./opencv_check"

screen -wipe
