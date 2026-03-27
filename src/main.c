/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tracker.h"

int main() {
    printf("=== DEBUT DU TEST DE PARSING ===\n\n");

    // Initialisation d'un faux environnement
    tracker_t* my_tracker = malloc(sizeof(tracker_t));

    peer_t *my_peer = initPeer("aaaaaaaaaaaaaaa", 2222);
    char response[256];

    // La commande textuelle exacte issue de l'exemple du sujet (et on y ajoute un leech pour tester la fin)
    // ATTENTION: strtok_r modifie la chaîne, il faut donc un tableau de char[] modifiable, pas un pointeur de chaîne littérale.
    char raw_command[] = "announce listen 2222 seed [file_a.dat 2097152 1024 8905e92afeb80fc7722ec89eb0bf0966 file_b.dat 3145728 1024 330a57722ec8b0bf09669a2b35f88e9e] leech [8905e92afeb80fc7722ec89eb0bf0966]";
    
    printf("Commande brute reçue : %s\n\n", raw_command);

    // Le dispatcher (ce qui se passe normalement avant d'appeler handle_announce)
    char* saveptr;
    char* first_word = strtok_r(raw_command, " \r\n", &saveptr);

    if (first_word && strcmp(first_word, "announce") == 0) {
        int status = handle_announce(my_tracker, my_peer, &saveptr, response);


        printf("\n=== RESULTATS ===\n");
        if (status == 0) {
            printf("Statut du parsing : SUCCES (0)\n");
            printf("Port d'écoute du pair extrait : %d\n", my_peer->listeningPort);
            printf("Buffer de réponse généré : %s", response);
        } else {
            printf("Statut du parsing : ECHEC (-1)\n");
            printf("Raison dans le buffer : %s", response);
        }

    } else {
        printf("Erreur : La commande n'est pas un announce.\n");
    }
    printf("%s, %d, %d, %s,\n", my_peer->seededFiles[0]->filename, my_peer->seededFiles[0]->length, my_peer->seededFiles[0]->piece_size, my_peer->seededFiles[0]->key);



    return 0;
}*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tracker.h"

int main() {
    printf("=== DEBUT DU TEST DE LA COMMANDE LOOK ===\n\n");

    
    // Initialisation du tracker (memset obligatoire pour éviter le segfault sur les cases vides)
    tracker_t* my_tracker = malloc(sizeof(tracker_t));
    memset(my_tracker, 0, sizeof(tracker_t));

    // Création d'un pair et ajout danrez votre fichier tracker.h et ajoutez simplement les "signatures" de vos s le tracker
    peer_t *my_peer = initPeer("192.168.1.50", 2222);
    my_tracker->peers[0] = my_peer;

    // Création de 3 fichiers de test
    // file_a correspond aux deux critères (bon nom, taille > 1Mo)
    file_t* file_a = initFile("file_a.dat", 2097152, "8905e92afeb80fc7722ec89eb0bf0966", 1024);
    
    // file_b a une bonne taille, mais pas le bon nom
    file_t* file_b = initFile("file_b.dat", 3145728, "330a57722ec8b0bf09669a2b35f88e9e", 1024);
    
    // file_c a le bon nom (pour tester le filtre), mais il est trop petit (< 1Mo)
    file_t* file_c = initFile("file_a.dat", 500000, "11111111111111111111111111111111", 1024);

    // On donne ces fichiers au pair en tant que SEED
    peerAddSeed(my_peer, file_a);
    peerAddSeed(my_peer, file_b);
    peerAddSeed(my_peer, file_c);

    printf("Environnement prêt : 1 Pair connecté, 3 fichiers en seed.\n\n");

    // ==========================================
    // 2. TEST DU PARSING ET DE LA RECHERCHE
    // ==========================================
    
    char response[512]; // Un peu plus grand au cas où la liste trouvée est longue

    // La commande textuelle exacte issue de l'exemple du sujet
    char raw_command[] = "look [filename=file_a.dat filesize>1048576]";
    
    printf("Commande brute reçue : %s\n\n", raw_command);

    // Le dispatcher
    char* saveptr;
    char* first_word = strtok_r(raw_command, " \r\n", &saveptr);

    if (first_word && strcmp(first_word, "look") == 0) {
        
        // Appel de votre fonction de recherche
        int status = handle_look(my_tracker, &saveptr, response);

        printf("\n=== RESULTATS ===\n");
        if (status == 0) {
            printf("Statut de la recherche : SUCCES (0)\n");
            printf("Buffer de réponse généré :\n%s", response);
        } else {
            printf("Statut de la recherche : ECHEC (-1)\n");
        }

    } else {
        printf("Erreur : La commande n'est pas un look.\n");
    }

    // Libération de la mémoire (bonne pratique)
    free(my_tracker);
    // freePeer(my_peer); // Si vous avez codé cette fonction

    return 0;
}