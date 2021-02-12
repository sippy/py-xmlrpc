#!/bin/sh

set -e

PYTHON_RCMD="`which "${PYTHON_CMD}"`"

${PYTHON_RCMD} setup.py build
sudo ${PYTHON_RCMD} setup.py install
