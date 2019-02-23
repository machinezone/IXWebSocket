/*
 *  devnull_server.js
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */
const WebSocket = require('ws');

let wss = new WebSocket.Server({ port: 5678, perMessageDeflate: true })

wss.on('connection', (ws) => {

  let handshake = false
  let authenticated = false

  ws.on('message', (data) => {

    console.log(data.toString('utf-8'))

    if (!handshake) {
      let response = {
          "action": "auth/handshake/ok",
          "body": {
              "data": {
                  "nonce": "MTI0Njg4NTAyMjYxMzgxMzgzMg==",
                  "version": "0.0.24"
              }
          },
          "id": 1
      }
      ws.send(JSON.stringify(response))
      handshake = true
    } else if (!authenticated) {
      let response = {
        "action": "auth/authenticate/ok",
        "body": {},
        "id": 2
      }
      
      ws.send(JSON.stringify(response))
      authenticated = true
    } else {
      console.log(data)
    }
  });
})
