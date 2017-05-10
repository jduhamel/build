#!/bin/bash
set -e

function error_exit
{
    echo "$1" 1>&2
    exit 1
}

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

BRANCH="develop"
TAG=""

GOPATH="$(pwd)/go"

libraries=(ball fault kani pubsub msgtest clog audit funky iq4app hl7 mongo cpet dicom fhir benchmarker)
executables=(dicomimporter fhirimporter hl7listener importer flow morph orca callout deidentify router sink squeal validator xlsimporter report)

mkdir -p $GOPATH/src/github.com/iq4health/


## Get dependencies

go get -u -v github.com/nats-io/gnatsd
go get -u -v github.com/nats-io/nats
go get -u -v gopkg.in/mgo.v2
go get -u -v github.com/denisenkom/go-mssqldb
go get -u -v github.com/tealeg/xlsx
go get -u -v github.com/lib/pq
go get -u -v gopkg.in/DATA-DOG/go-sqlmock.v1
go get -u -v github.com/minio/minio-go

cd $GOPATH/src/github.com/iq4health

for lib in "${libraries[@]}"
do
    git clone -b develop git@github.com:iq4health/$lib.git
done

for lib in "${libraries[@]}"
do
    cd $lib
    go test -v
    cd ..
done

for lib in "${executables[@]}"
do
    git clone -b develop git@github.com:iq4health/$lib.git
done

for lib in "${executables[@]}"
do
    cd $lib
    go test -v
    cd ..
done



