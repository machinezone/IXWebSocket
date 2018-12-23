#!/bin/sh

endpoint="ws://127.0.0.1:8765"
endpoint="ws://127.0.0.1:5678"
appkey="appkey"
channel="foo"
rolename="a_role"
rolesecret="a_secret"
filename=${FILENAME:=events.jsonl}

build/cobra_publisher $endpoint $appkey $channel $rolename $rolesecret $filename
