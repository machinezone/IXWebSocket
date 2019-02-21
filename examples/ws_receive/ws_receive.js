/*
 *  ws_receive.js
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */
const WebSocket = require('ws')
const djb2 = require('djb2')
const fs = require('fs')

const wss = new WebSocket.Server({ port: 8080,
                                   perMessageDeflate: false,
                                   maxPayload: 1024 * 1024 * 1024 * 1024});

wss.on('connection', function connection(ws) {
  ws.on('message', function incoming(data) {
    console.log('Received message')

    let str = data.toString()
    let obj = JSON.parse(str)

    console.log(obj.id)
    console.log(obj.djb2_hash)
    console.log(djb2(obj.content))

    var content = Buffer.from(obj.content, 'base64')
    // let bytes = base64.decode(obj.content)

    let path = obj.filename
    fs.writeFile(path, content, function(err) {
      if (err) {
        throw err
      } else {
        console.log('wrote data to disk')
      }
    });

    let response = {
      id: obj.id
    }

    ws.send(JSON.stringify(response))
  });
});
