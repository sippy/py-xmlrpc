#!/bin/sh

set -e

PYTHON_CMD="${PYTHON_CMD:-"python"}"
PYTHON_VER="${PYTHON_VER:-"2.7"}"

if [ "${PYTHON_VER}" != "2.7" ]
then
  echo "Not Yet!"
  exit 0
fi

export PYTHONPATH="/usr/local/lib/python${PYTHON_VER}/dist-packages"

nfails=0

run_tests() {
  for ex in ${@}
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
}

run_tests base64 emptyString build amper date ascii encode exception

${PYTHON_CMD} examples/examples.py server&
sleep 1
run_tests authClient fault client requestExit

if ! wait
then
  nfails=$((${nfails} + 1))
fi

exit ${nfails}
