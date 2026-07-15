#include <functions/manager.c>

/* compare colliding sessions */

int
main()
{
    manager_t m = {
        .itad = 10,
        .id = 0
    };

    inet_pton(AF_INET, "10.0.0.0", &m.id);

    peer_t peer = {
        .itad = 20
    };

    session_t s1 = {
        .initiated = 1,
        .peer = &peer
    }, s2 = {
        .initiated = 0,
        .peer = &peer
    };

    inet_pton(AF_INET, "20.0.0.0", &s1.id);
    inet_pton(AF_INET, "20.0.0.0", &s2.id);

    return manager_collision_sessions_compare(&m, &s1, &s2) != 1;
}

