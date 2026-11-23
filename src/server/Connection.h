#pragma once
#include "Net.h"
#include "../query/QueryEngine.h"

// One client of the binary protocol: read MSG_QUERY, run it, reply, repeat.
void handle_binary_client(SOCKET fd, QueryEngine &engine);

// One HTTP request (Connection: close): read it, dispatch to the API, reply.
class ApiHandler;
void handle_http_client(SOCKET fd, ApiHandler &api);
