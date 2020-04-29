webSocket = WebSocket.new()

webSocket:setUrl("ws://localhost:8008")
print("Url: " .. webSocket:getUrl())

webSocket:start()

while true
do
    print("Waiting ...")
    webSocket:send("coucou");
    webSocket:sleep(1000)
end
