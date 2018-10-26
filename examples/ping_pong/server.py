#!/usr/bin/env python

import os
import asyncio
import websockets

async def echo(websocket, path):
    async for message in websocket:
        print(message)
        await websocket.send(message)

        if os.getenv('TEST_CLOSE'):
            print('Closing')
            # breakpoint()
            await websocket.close(1001, 'close message')
            # await websocket.close()
            break

asyncio.get_event_loop().run_until_complete(
    websockets.serve(echo, 'localhost', 5678))
asyncio.get_event_loop().run_forever()
