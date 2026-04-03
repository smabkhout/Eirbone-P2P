#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_CLIENTS 50
#define BUFFER_SIZE 4096
#define MAX_FILES_PER_PEER 100
#define KEY_SIZE 33 // 32 caractères pour le MD5 + 1 pour '\0' [cite: 68, 83]

// Structure pour représenter un fichier partagé (seeder) [cite: 14]
typedef struct {
    char filename[256];
    long length;
    int piece_size; // Normalement fixé à 1024 [cite: 27]
    char key[KEY_SIZE]; // Hash MD5 [cite: 68, 83]
} SeededFile;

// Structure pour représenter un fichier en cours de téléchargement (leecher) [cite: 15]
typedef struct {
    char key[KEY_SIZE]; // Pour les leechers, on n'annonce que la clé [cite: 73]
} LeechedFile;

// Structure complète pour garder l'état de chaque client connecté
typedef struct {
    int fd;                             // Descripteur de la socket (0 si inactif)
    int error_count;                    // Compteur d'erreurs de formatage 
    char buffer[BUFFER_SIZE];           // Tampon pour le message framing [cite: 135, 137]
    int buffer_len;                     // Nombre d'octets actuellement dans le tampon
    
    // Informations réseau du pair
    char ip[INET_ADDRSTRLEN];           // Adresse IP du client
    int listening_port;                 // Port d'écoute annoncé par le pair [cite: 73, 75]
    
    // Fichiers partagés par ce pair
    SeededFile seeded_files[MAX_FILES_PER_PEER];
    int seeded_count;
    
    // Fichiers téléchargés par ce pair
    LeechedFile leeched_files[MAX_FILES_PER_PEER];
    int leeched_count;
} ClientState;

ClientState clients[MAX_CLIENTS];

// Initialisation du tableau des clients
void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].error_count = 0;
        clients[i].buffer_len = 0;
        clients[i].listening_port = 0;
        clients[i].seeded_count = 0;
        clients[i].leeched_count = 0;
        memset(clients[i].buffer, 0, BUFFER_SIZE);
        memset(clients[i].ip, 0, INET_ADDRSTRLEN);
    }
}

// Fonction de traitement d'une commande (à étoffer avec le parsing réel)
int process_command(char *command, ClientState *client) {
    printf("[Client %s:%d] Commande : %s\n", client->ip, client->listening_port, command);

    // TODO: Utiliser strtok_r ou sscanf pour analyser les commandes
    if (strncmp(command, "announce", 8) == 0) {
        // Logique d'extraction des fichiers seed/leech à faire ici [cite: 73]
        send(client->fd, "ok\n", 3, 0); // Les messages se terminent par \n [cite: 24]
        return 0; // Succès
    } 
    else if (strncmp(command, "look", 4) == 0) {
        char *response = "list [file_a.dat 2097152 1024 8905e92afeb80fc7722ec89eb0bf0966]\n"; [cite: 92]
        send(client->fd, response, strlen(response), 0);
        return 0;
    }
    else if (strncmp(command, "getfile", 7) == 0) {
        char *response = "peers 8905e92afeb80fc7722ec89eb0bf0966 [127.0.0.1:2222]\n"; [cite: 101]
        send(client->fd, response, strlen(response), 0);
        return 0;
    }
    else if (strncmp(command, "update", 6) == 0) {
        send(client->fd, "ok\n", 3, 0); [cite: 171]
        return 0;
    }

    // Commande non reconnue ou mal formatée
    return -1; 
}

int main(int argc, char *argv[]) {
    int master_socket, new_socket, activity, valread, sd, max_sd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    fd_set readfds;

    // Le port devrait idéalement être lu depuis config.ini [cite: 182]
    int port = 12345; 

    init_clients();

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Échec de création du socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Échec du bind");
        exit(EXIT_FAILURE);
    }

    if (listen(master_socket, 10) < 0) {
        perror("Échec du listen");
        exit(EXIT_FAILURE);
    }

    printf("Tracker démarré. En attente de connexions sur le port %d...\n", port);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Ajouter les sockets clients actifs à l'ensemble
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Attente d'une activité sur l'un des sockets (mécanisme de multiplexage) [cite: 33]
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Erreur select");
            continue;
        }

        // 1. GESTION DES NOUVELLES CONNEXIONS
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("Erreur accept");
                continue;
            }

            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address.sin_addr), ip_str, INET_ADDRSTRLEN);
            printf("Nouvelle connexion acceptée : IP %s, Socket FD %d\n", ip_str, new_socket);

            // Trouver un emplacement libre
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    strncpy(clients[i].ip, ip_str, INET_ADDRSTRLEN);
                    clients[i].error_count = 0;
                    clients[i].buffer_len = 0;
                    break;
                }
            }
        }

        // 2. GESTION DES REQUÊTES DES CLIENTS EXISTANTS
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;

            if (FD_ISSET(sd, &readfds)) {
                // On lit directement à la fin des données déjà présentes dans le buffer
                int space_left = BUFFER_SIZE - clients[i].buffer_len - 1;
                
                if (space_left <= 0) {
                    printf("Buffer plein pour le client FD %d, fermeture de connexion.\n", sd);
                    close(sd);
                    clients[i].fd = 0;
                    continue;
                }

                valread = read(sd, clients[i].buffer + clients[i].buffer_len, space_left);

                if (valread == 0) {
                    // Déconnexion propre
                    printf("Client FD %d déconnecté.\n", sd);
                    close(sd);
                    clients[i].fd = 0;
                } else if (valread > 0) {
                    clients[i].buffer_len += valread;
                    clients[i].buffer[clients[i].buffer_len] = '\0'; // Sécurité pour les fonctions str*

                    // Recherche de commandes complètes (terminées par \n) [cite: 24, 139]
                    char *newline_pos;
                    while ((newline_pos = strchr(clients[i].buffer, '\n')) != NULL) {
                        *newline_pos = '\0'; // Isole la commande
                        
                        // Nettoyage du \r éventuel [cite: 197]
                        if (newline_pos > clients[i].buffer && *(newline_pos - 1) == '\r') {
                            *(newline_pos - 1) = '\0';
                        }

                        // Traitement de la commande
                        int status = process_command(clients[i].buffer, &clients[i]);
                        
                        // Gestion de la règle des 3 erreurs 
                        if (status == -1) {
                            clients[i].error_count++;
                            printf("Erreur de formatage client FD %d (Tentative %d/3)\n", sd, clients[i].error_count);
                            
                            if (clients[i].error_count >= 3) {
                                printf("Fermeture de la connexion avec le client FD %d (3 erreurs consécutives)\n", sd);
                                close(sd);
                                clients[i].fd = 0;
                                break; 
                            }
                        } else {
                            clients[i].error_count = 0; // Réinitialise si la commande est correcte
                        }

                        // Décalage du buffer pour les données restantes (Message Framing) [cite: 137, 138]
                        int command_len = (newline_pos - clients[i].buffer) + 1;
                        int remaining = clients[i].buffer_len - command_len;
                        
                        if (remaining > 0) {
                            memmove(clients[i].buffer, newline_pos + 1, remaining);
                        }
                        clients[i].buffer_len = remaining;
                        clients[i].buffer[remaining] = '\0';
                    }
                }
            }
        }
    }
    return 0;
}