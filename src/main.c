#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tracker.h"

int main() {
    printf("=== DEBUT DU TEST DE PARSING ===\n\n");

    // Initialisation d'un faux environnement
    tracker_t my_tracker;
    peer_t my_peer;
    char response[256];

    // La commande textuelle exacte issue de l'exemple du sujet (et on y ajoute un leech pour tester la fin)
    // ATTENTION: strtok_r modifie la chaîne, il faut donc un tableau de char[] modifiable, pas un pointeur de chaîne littérale.
    char raw_command[] = "announce listen 2222 seed [file_a.dat 2097152 1024 8905e92afeb80fc7722ec89eb0bf0966 file_b.dat 3145728 1024 330a57722ec8b0bf09669a2b35f88e9e] leech [8905e92afeb80fc7722ec89eb0bf0966]";
    
    printf("Commande brute reçue : %s\n\n", raw_command);

    // Le dispatcher (ce qui se passe normalement avant d'appeler handle_announce)
    char* saveptr;
    char* first_word = strtok_r(raw_command, " \r\n", &saveptr);

    if (first_word && strcmp(first_word, "announce") == 0) {
        
        // Appel de votre fonction
        int status = handle_announce(&my_tracker, &my_peer, &saveptr, response);

        printf("\n=== RESULTATS ===\n");
        if (status == 0) {
            printf("Statut du parsing : SUCCES (0)\n");
            printf("Port d'écoute du pair extrait : %d\n", my_peer.listeningPort);
            printf("Buffer de réponse généré : %s", response);
        } else {
            printf("Statut du parsing : ECHEC (-1)\n");
            printf("Raison dans le buffer : %s", response);
        }

    } else {
        printf("Erreur : La commande n'est pas un announce.\n");
    }

    return 0;
}