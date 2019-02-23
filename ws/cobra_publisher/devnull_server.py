#!/usr/bin/env python

import os
import json
import asyncio
import websockets


async def echo(websocket, path):
    handshake = False
    authenticated = False

    async for message in websocket:
        print(message)

        if not handshake:
            response = {
                "action": "auth/handshake/ok",
                "body": {
                    "data": {
                        "nonce": "MTI0Njg4NTAyMjYxMzgxMzgzMg==",
                        "version": "0.0.24"
                    }
                },
                "id": 1
            }
            await websocket.send(json.dumps(response))
            handshake = True

        elif not authenticated:
            response = {
                "action": "auth/authenticate/ok",
                "body": {},
                "id": 2
            }
          
            await websocket.send(json.dumps(response))
            authenticated = True


asyncio.get_event_loop().run_until_complete(
    websockets.serve(echo, 'localhost', 5678))
asyncio.get_event_loop().run_forever()
