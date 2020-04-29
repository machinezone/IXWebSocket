--
-- make luarocks && (cd luarocks ; ../build/luarocks/luarocks)
--
local webSocket = WebSocket.new()

webSocket:setUrl("ws://localhost:8008")
print("Url: " .. webSocket:getUrl())

-- Start the background thread
webSocket:start()

local i = 0

while true
do
    print("Sending message...")
    webSocket:send("msg_" .. tostring(i));
    i = i + 1

    print("Waiting 1s...")
    webSocket:sleep(1000)

    if webSocket:hasMessage() then
        local msg = webSocket:getMessage()
        print("Received: " .. msg)
        webSocket:popMessage()
    end
end
