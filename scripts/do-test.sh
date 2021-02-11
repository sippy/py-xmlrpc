#!/bin/sh

set -e

PYTHON_CMD="${PYTHON_CMD:-"python"}"

export PYTHONPATH="/usr/local/lib/python2.7/dist-packages"

nfails=0
for ex in base64 emptyString build amper date ascii encode
do
  echo -n "${ex}: "
  res="ok"
  if ! ${PYTHON_CMD} examples/examples.py ${ex} >/dev/null
  then
    res="FAIL"
    nfails=$((${nfails} + 1))
  fi
  echo $res
done
exit ${nfails}
