// net_client_c_api.cpp (extern "C")

static NetClient* s_client_instance = nullptr;

extern "C" void net_client_init(event_bus_t *bus) {
    if (!s_client_instance) {
        s_client_instance = new NetClient(bus);
    }
}

extern "C" void net_client_start(void) {
    if (s_client_instance) s_client_instance->start();
}

// ... mapper les autres fonctions ...
