#!/bin/sh

set -e

sudo find / -name xmlrpc -type d

PYTHON_CMD="${PYTHON_CMD:-"python"}"

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
