#include <cbor.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <cmath>
#include <iostream>
/*
Determine the center of the circle:

We know two points on the circle: (20, 45) and (50, 25)
The midpoint of these two points will give us the center of the circle:
Center_x = (20 + 50) / 2 = 35
Center_y = (45 + 25) / 2 = 35
Calculate the radius of the circle:

We can use the distance formula between one of the points and the center:
Distance = sqrt((50 - 35)^2 + (25 - 35)^2) = sqrt(250)
Radius = Distance = sqrt(250)
Calculate the angle of the arc:

We can use trigonometry to find the angle of the arc between the two points:
Let A = (20, 45), B = (50, 25), and C = (35, 35) (the center of the circle)
We can use the dot product formula to find the angle between vectors AC and BC:
cos(theta) = (AC . BC) / (|AC| * |BC|)
AC = (20 - 35, 45 - 35) = (-15, 10)
BC = (50 - 35, 25 - 35) = (15, -10)
AC . BC = (-15 * 15) + (10 * -10) = -275
|AC| = sqrt((-15)^2 + 10^2) = sqrt(325)
|BC| = sqrt(15^2 + (-10)^2) = sqrt(325)
cos(theta) = -275 / (sqrt(325) * sqrt(325)) = -11/13
theta = acos(-11/13) = 126.87 degrees
*/
const int port = 6789;
const auto delay = std::chrono::milliseconds(50); // attente de 50ms entre chaque mise envoi
const double period = 1000;
const double pi = 4 * atan(1);

int main(int argc, char const *argv[]) {
    double sun_x;
    double sun_y;
    char const *host;
    if (argc < 2)
        host = "localhost";
    else
        host = argv[1];

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    int socket_info;
    struct addrinfo *server;
    // int found = getaddrinfo("localhost", "6789", &hints, &server);
    int found = getaddrinfo("127.0.0.1", "6789", &hints, &server);
    if (found < 0)
        perror("erreur recherche hôte");

    double time = 0;
    unsigned char *buffer;
    int iter = 0;

    double x_initial = 25;
    double y_initial = 75;
    double x_middle = 50;
    double y_middle = 25;
    double x_end = 75;
    double y_end = 75;
    double a1 = (y_initial - y_middle) / (x_initial - x_middle);
    double b1 = y_initial - a1 * x_initial;
    double a2 = (y_middle - y_end) / (x_middle - x_end);
    double b2 = y_middle - a2 * x_middle;
    double delta = 0.1;
    sun_x = x_initial;
    sun_y = y_initial;
    while (true) {
        std::this_thread::sleep_for(delay);
        cbor_item_t *cbor_root = cbor_new_definite_map(2);
        sun_x += delta;
        if (sun_x > x_middle) {
            sun_y = a2 * sun_x + b2;
        } else  {
            sun_y = a1 * sun_x + b1;
        } 
        
         if (sun_x > x_end) {
            break;
        }
        std::cout << "x: " << sun_x << " y: " << sun_y << std::endl;

        cbor_map_add(cbor_root, (struct cbor_pair) {
                .key = cbor_move(cbor_build_string("sun_x")),
                .value = cbor_move(cbor_build_float8(sun_x ))
        });
        cbor_map_add(cbor_root, (struct cbor_pair) {
                .key = cbor_move(cbor_build_string("sun_y")),
                .value = cbor_move(cbor_build_float8(sun_y ))
        });
        size_t buffer_size,
                length = cbor_serialize_alloc(cbor_root, &buffer, &buffer_size);
        sendto(socket_fd, buffer, length, 0, server->ai_addr, server->ai_addrlen);

        free(buffer);
        cbor_decref(&cbor_root);
    }
    free(server);
}