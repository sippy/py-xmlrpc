#!/bin/sh

sudo find / -name xmlrpc -type d
sudo find / -name xmlrpc.py -type f

set -e

PYTHON_CMD="${PYTHON_CMD:-"python"}"

nfails=0
for ex in base64 emptyString build amper date ascii encode
do
  echo -n "${ex}: "
  res="ok"
  if ! strace -o strace.out ${PYTHON_CMD} examples/examples.py ${ex} >/dev/null
  then
    res="FAIL"
    nfails=$((${nfails} + 1))
    cat strace.out >&2
  fi
  echo $res
done
exit ${nfails}
