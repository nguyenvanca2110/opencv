#!/bin/bash

ps fax|grep "opencv_check" | awk '{}{print $0; system("kill -9 " $1);}'
