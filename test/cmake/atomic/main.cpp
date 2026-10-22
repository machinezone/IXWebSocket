/*
 *  Check that installed consumers can link the library's 64-bit atomic users.
 */
#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXWebSocketTransport.h>

int main()
{
    ix::ConnectionState first;
    ix::ConnectionState second;
    ix::WebSocketTransport transport;
    return first.getId() == second.getId() ||
           transport.getReadyState() != ix::WebSocketTransport::ReadyState::CLOSED;
}
