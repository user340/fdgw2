#!/bin/sh

export LC_ALL=C LANG=C

OS=`uname`

if [ !`which lndir` ]; then
	NOT_EXIST_lndir=1
fi

if [ !`which bmake` ]; then
	NOT_EXIST_bmake=1
fi

if [ $OS = 'Linux' ]; then
	if [ `which apt-get` ]; then
		if [ $NOT_EXIST_bmake ]; then
			apt-get install bmake
		fi
		if [ $NOT_EXIST_lndir ]; then
			apt-get install xutils-dev
		fi
	elif [ `which yum` ]; then
		if [ $NOT_EXIST_bmake ]; then
			# 失敗する。bmakeをインストールするには？
			yum install bmake
		fi
		if [ $NOT_EXIST_lndir ]; then
			yum install imake
		fi
	fi
fi
