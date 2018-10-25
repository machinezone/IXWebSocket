#!/usr/bin/env python

import asyncio
import websockets

async def hello(uri):
    async with websockets.connect(uri) as websocket:
        await websocket.send("Hello world!")
        response = await websocket.recv()
        print(response)

        pong_waiter = await websocket.ping('coucou')
        ret = await pong_waiter   # only if you want to wait for the pong
        print(ret)

asyncio.get_event_loop().run_until_complete(
    hello('ws://localhost:5678'))
