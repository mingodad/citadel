#!/bin/bash

xgettext \
	--copyright-holder='The Citadel Project - http://www.citadel.org' \
	-k_ \
	-o webcit.pot \
	../*.c
