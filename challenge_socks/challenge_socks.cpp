#include <asio.hpp>
#include <iostream>
#include <array>
#include <functional>

using asio::ip::tcp;

const uint8_t SOCKS_VERSION = 5;
const std::string SOCKS_ADDR = "127.0.0.1";
const uint16_t SOCKS_PORT = 1080;

void handle_client(tcp::socket& client_socket, asio::io_context& io_context) {
    try {

        // Lecture du header Sock
        std::array<uint8_t, 2> header;
        asio::read(client_socket, asio::buffer(header));

        uint8_t version = header[0];
        uint8_t nmethods = header[1];

        // Check si on est bien sur un protocole Sockv5
        assert(version == SOCKS_VERSION);
        assert(nmethods > 0);

        std::cout << "SOCKS Version: " << (int)version << ", Number of Methods: " << (int)nmethods << std::endl;

        // Lecture de la méthode d'authentification
        std::vector<uint8_t> methods(nmethods);
        asio::read(client_socket, asio::buffer(methods));

        // Envoie de la réponse
        // 0 => no authentification requiered
        std::array<uint8_t, 2> response = { SOCKS_VERSION, 0 };
        asio::write(client_socket, asio::buffer(response));

        // Lecture de la requête du client
        // Format : (version, command, reserved, address type)
        std::array<uint8_t, 4> request;
        asio::read(client_socket, asio::buffer(request));

        uint8_t cmd = request[1];
        uint8_t address_type = request[3];
        assert(version == SOCKS_VERSION);

        std::string address;
        uint16_t port = 0;

        // Si l'adresse est de type IPv4
        if (address_type == 1) { 
            std::array<uint8_t, 4> ip_address;
            asio::read(client_socket, asio::buffer(ip_address));
            address = std::to_string(ip_address[0]) + "." + std::to_string(ip_address[1]) + "." + std::to_string(ip_address[2]) + "." + std::to_string(ip_address[3]);
            asio::read(client_socket, asio::buffer(&port, 2));
            port = ntohs(port);
        }

        // Si l'adresse est un nom de domaine
        else if (address_type == 3) { 
            uint8_t domain_length;
            asio::read(client_socket, asio::buffer(&domain_length, 1));
            std::vector<uint8_t> domain(domain_length);
            asio::read(client_socket, asio::buffer(domain));
            address = std::string(domain.begin(), domain.end());
            asio::read(client_socket, asio::buffer(&port, 2));
            port = ntohs(port);
        }


        //Implementation de la commande Connect
        if (cmd == 1) {
            try {

                //Connection au server proxy
                tcp::socket remote_socket(io_context);
                asio::connect(remote_socket, asio::ip::tcp::resolver(io_context).resolve(address, std::to_string(port)));


                //Envoie de la réponse success au client
                std::array<uint8_t, 10> success_reply = {
                    SOCKS_VERSION, 0, 0, 1, 0, 0, 0, 0, 0, 0
                };
                asio::write(client_socket, asio::buffer(success_reply));

                std::array<char, 4096> buffer;
                asio::strand<asio::io_context::executor_type> strand(io_context.get_executor());

                std::function<void()> read_from_remote;
                std::function<void()> read_from_client;


                // Fonction pour lire les données du server proxy vers le client 
                read_from_remote = [&]() {
                    remote_socket.async_read_some(asio::buffer(buffer), [&](std::error_code ec, size_t n) {
                        if (!ec) {
                            asio::async_write(client_socket, asio::buffer(buffer, n), [&](std::error_code ec, size_t n) {
                                if (!ec) {
                                    read_from_remote();
                                }
                                });
                        }
                        });
                    };

                // Fonction pour lire les données du client vers le server proxy
                read_from_client = [&]() {
                    client_socket.async_read_some(asio::buffer(buffer), [&](std::error_code ec, size_t n) {
                        if (!ec) {
                            asio::async_write(remote_socket, asio::buffer(buffer, n), [&](std::error_code ec, size_t n) {
                                if (!ec) {
                                    read_from_client();
                                }
                                });
                        }
                        });
                    };

                // Démarrage de la communication bidirectionnel
                read_from_remote();
                read_from_client();

                io_context.run();

            }
            catch (const std::exception& e) {
                // En cas d'erreur, envoyer une réponse d'échec au client
                std::cerr << "Connection failed: " << e.what() << std::endl;
                std::array<uint8_t, 10> failure_reply = {
                    SOCKS_VERSION, 5, 0, 0, 0, 0, 0, 0, 0, 0
                };
                asio::write(client_socket, asio::buffer(failure_reply));
            }
        }
        else {
            // Si la commande n'est pas supportée, envoyer une réponse d'échec
            std::array<uint8_t, 10> failure_reply = {
                SOCKS_VERSION, 7, 0, 0, 0, 0, 0, 0, 0, 0
            };
            asio::write(client_socket, asio::buffer(failure_reply));
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Fonction Pour lancer le server proxy SOCKS
void run_server(asio::io_context& io_context) {
    // Initialisation de l'accepteur sur l'adresse et le port spécifiés
    tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::make_address(SOCKS_ADDR), SOCKS_PORT));
    std::cout << "Proxy server running on " << SOCKS_ADDR << ":" << SOCKS_PORT << std::endl;


    while (true) {
        tcp::socket client_socket(io_context);

        // Attente et acceptation d'une nouvelle connexion client
        acceptor.accept(client_socket);

        // Création d'un thread pour gérer chaque client de manière indépendante
        auto handle_client_lambda = [](tcp::socket new_client, asio::io_context& io_context) {
            handle_client(new_client, io_context);
            };

        std::thread(handle_client_lambda, std::move(client_socket), std::ref(io_context)).detach();
    }
}

int main() {
    try {
        // Initialisation du contexte
        asio::io_context io_context;
        run_server(io_context);
    }
    catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    return 0;
}
