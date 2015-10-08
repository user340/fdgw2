#!/bin/sh

PWD= $1

mkdir ${PWD}/objects || echo "WARNING: exist ${PWD}/objects"; exit 1
mkdir ${PWD}/tools || echo "WARNING: exist ${PWD}/tools"; exit 1
