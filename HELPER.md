# 📘 ft_irc — Guide Complet



---



## Table des matières



1. [Vue d'ensemble](#1-vue-densemble)

2. [Comment compiler et lancer](#2-comment-compiler-et-lancer)

3. [Architecture du code](#3-architecture-du-code)

4. [Les 4 classes du projet](#4-les-4-classes-du-projet)

5. [Le cœur réseau : poll() et sockets non-bloquants](#5-le-cœur-réseau--poll-et-sockets-non-bloquants)

6. [Cycle de vie d'une connexion](#6-cycle-de-vie-dune-connexion)

7. [Enregistrement (PASS / NICK / USER)](#7-enregistrement-pass--nick--user)

8. [Commandes supportées (détail)](#8-commandes-supportées-détail)

9. [Les modes de channel (MODE)](#9-les-modes-de-channel-mode)

10. [Gestion mémoire et signaux](#10-gestion-mémoire-et-signaux)

11. [Questions pièges fréquentes](#11-questions-pièges-fréquentes)

12. [Répartition en 2 parties](#12-répartition-en-2-parties)



---



## 1. Vue d'ensemble



**ft_irc** est un **serveur IRC** (Internet Relay Chat) écrit en **C++98**. Il gère plusieurs clients simultanément avec une **seule boucle `poll()`** et des **sockets non-bloquants**. Le protocole suivi est basé sur les **RFC 1459 et 2812**.



**Ce que fait le serveur :**

- Accepte des connexions TCP de clients IRC (netcat, irssi, HexChat, etc.)

- Authentifie les clients avec un mot de passe

- Permet de créer/rejoindre des channels (salons)

- Envoie des messages privés ou dans des channels

- Gère des opérateurs de channel avec des commandes d'administration



**Ce que le serveur ne fait PAS (pas demandé par le sujet) :**

- Pas de communication serveur-à-serveur

- Pas de bot (sauf bonus)

- Pas de file transfer (sauf bonus)



---



## 2. Comment compiler et lancer



```bash

# Compilation

make            # Compile le binaire 'ircserv'

make re         # Recompile tout

make clean      # Supprime les .o

make fclean     # Supprime .o + binaire



# Lancement

./ircserv <port> <password>

# Exemple :

./ircserv 6667 mypass



# Test avec netcat

nc 127.0.0.1 6667

PASS mypass

NICK john

USER john 0 * :John

JOIN #general

PRIVMSG #general :hello everyone

```



**Flags de compilation :** `-Wall -Wextra -Werror -std=c++98`



---



## 3. Architecture du code



```

srcs/

├── main.cpp                  → Point d'entrée, parsing args, signaux

├── IRCCore.cpp               → Classe principale : socket, boucle poll(), connexions

├── IRCCoreHelpers.cpp        → Fonctions utilitaires du serveur (parsing, envoi, lookup)

├── Session.cpp               → Représente UN client connecté

├── Room.cpp                  → Représente UN channel IRC

├── helpers.cpp               → Utilitaires globaux (non-blocking, fatal)

└── commands/

    ├── Dispatcher.cpp        → Aiguillage des commandes (PASS → cmdPass, etc.)

    ├── Registration.cpp      → PASS, NICK, USER, QUIT

    ├── RoomCommands.cpp      → JOIN, PART

    ├── Messaging.cpp         → PRIVMSG

    └── AdminCommands.cpp     → KICK, INVITE, TOPIC, MODE

```



---



## 4. Les 4 classes du projet



### 4.1. `IRCCore` — Le serveur



C'est la classe **centrale**. Elle contient :



| Attribut | Type | Rôle |

|----------|------|------|

| `_listenSock` | `int` | Socket d'écoute du serveur |

| `_portNum` | `int` | Port TCP |

| `_secret` | `string` | Mot de passe serveur |

| `_hostname` | `string` | Nom du serveur (`"ft_irc"`) |

| `_sessions` | `map<int, Session*>` | Tous les clients connectés (clé = fd) |

| `_rooms` | `map<string, Room*>` | Tous les channels (clé = nom comme `#general`) |

| `_watchers` | `vector<pollfd>` | Tableau de file descriptors surveillés par `poll()` |

| `_active` | `bool` | Le serveur tourne-t-il encore ? |



**Méthodes clés :**

- `initSocket()` → Crée le socket, bind, listen, met en non-bloquant

- `loop()` → **LA** boucle principale avec `poll()`

- `onIncomingConnection()` → Accepte un nouveau client

- `onDataAvailable()` → Lit les données d'un client

- `onReadyToSend()` → Envoie les données buffered à un client

- `dropConnection()` → Déconnecte un client proprement

- `dispatch()` → Parse une ligne reçue et appelle la bonne commande

- `shutdown()` → Ferme tout (libère mémoire, ferme sockets)



### 4.2. `Session` — Un client



Représente **un client connecté**. Chaque client a :



| Attribut | Type | Rôle |

|----------|------|------|

| `_sockFd` | `int` | File descriptor de la socket du client |

| `_nick` | `string` | Nickname IRC |

| `_user` | `string` | Username IRC |

| `_recvBuf` | `string` | Buffer de réception (données pas encore traitées) |

| `_outBuf` | `string` | Buffer d'envoi (données à envoyer au client) |

| `_passOk` | `bool` | Le client a-t-il donné le bon mot de passe ? |

| `_welcomed` | `bool` | Le client est-il complètement enregistré ? |



**Cycle d'un client :**

1. Connexion TCP → Session créée avec `_passOk = false`, `_welcomed = false`

2. Envoie `PASS` → `_passOk = true` si correct

3. Envoie `NICK` + `USER` → `_welcomed = true`, reçoit le message 001 de bienvenue

4. Peut maintenant utiliser toutes les commandes



### 4.3. `Room` — Un channel



Représente **un channel IRC** (ex: `#general`). Chaque Room a :



| Attribut | Type | Rôle |

|----------|------|------|

| `_label` | `string` | Nom du channel (ex: `#general`) |

| `_subject` | `string` | Topic du channel |

| `_passphrase` | `string` | Mot de passe du channel (mode +k) |

| `_users` | `vector<Session*>` | Liste des users dans le channel |

| `_admins` | `vector<Session*>` | Liste des opérateurs du channel |

| `_guestList` | `vector<Session*>` | Liste des invités (mode +i) |

| `_restricted` | `bool` | Mode invite-only (+i) actif ? |

| `_lockedSubject` | `bool` | Seuls les ops peuvent changer le topic (+t) ? |

| `_maxUsers` | `int` | Limite d'utilisateurs (+l), 0 = pas de limite |



**Points importants :**

- Le **premier user** à rejoindre un channel en devient automatiquement **opérateur**

- Quand le dernier user quitte un channel, **le channel est supprimé** (delete + erase de la map)

- `relay()` envoie un message à tous les users **sauf** un (l'expéditeur)

- `relayAll()` envoie à **tout le monde** (utilisé pour JOIN, KICK, etc.)



### 4.4. Helpers



Deux fonctions globales simples :

- `enable_nonblock(fd)` → Met un fd en mode non-bloquant avec `fcntl(fd, F_SETFL, O_NONBLOCK)`

- `fatal(msg)` → Affiche une erreur et quitte avec `exit(1)`



---



## 5. Le cœur réseau : poll() et sockets non-bloquants



### Pourquoi poll() ?



Le sujet interdit d'avoir **un thread par client**. On doit gérer **tous les clients dans un seul thread**. `poll()` permet de surveiller **plusieurs file descriptors** en même temps et d'être notifié quand :

- Un client a envoyé des données (`POLLIN`)

- Un client est prêt à recevoir des données (`POLLOUT`)

- Un client s'est déconnecté (`POLLHUP`, `POLLERR`)



### Pourquoi non-bloquant ?



Si `recv()` ou `send()` bloque, **tout le serveur est bloqué** pour tous les clients. Avec des sockets non-bloquants :

- `recv()` retourne immédiatement même s'il n'y a rien à lire

- `send()` retourne immédiatement même si le buffer noyau est plein



### Le flux de la boucle principale (`loop()`)



```

1. poll() attend un événement sur n'importe quel fd

2. Pour chaque fd ayant un événement :

   a. Si c'est le socket d'écoute + POLLIN → accepter un nouveau client

   b. Si c'est un client + POLLIN → lire les données, parser, dispatcher

   c. Si c'est un client + POLLOUT → envoyer les données buffered

   d. Si POLLERR/POLLHUP → déconnecter le client

3. Retour en 1

```



### Buffering des données



- **Réception** : les données sont ajoutées à `_recvBuf` de la Session. On cherche un `\n` pour découper en lignes complètes. Chaque ligne complète est dispatchée comme une commande.

- **Envoi** : les réponses sont ajoutées à `_outBuf` de la Session. Quand `poll()` signale POLLOUT, on envoie ce qu'on peut avec `send()`. On retire les octets envoyés avec `drainOutBuf()`.

- **Protection anti-flood** : si `_recvBuf` dépasse 4096 octets sans `\n`, le client est déconnecté.



---



## 6. Cycle de vie d'une connexion



```

Client se connecte (TCP)

    → accept() → nouveau fd → nouvelle Session

    → ajouté à _sessions et _watchers



Client envoie des données

    → recv() → feedRecvBuf() → on cherche des \n

    → chaque ligne complète → dispatch()



dispatch() parse la commande :

    → "PASS mypass"  → verbe = "PASS", args = "mypass"

    → appelle cmdPass(), cmdNick(), cmdJoin(), etc.



Les réponses sont buffered :

    → enqueueReply() → pushToOutBuf() → refreshPollFlags()

    → POLLOUT activé → onReadyToSend() → send() → drainOutBuf()



Client se déconnecte ou erreur :

    → dropConnection() → purgeFromRooms() → close(fd) → delete Session

```



---



## 7. Enregistrement (PASS / NICK / USER)



L'enregistrement est **obligatoire** avant de pouvoir utiliser les autres commandes.



### Ordre requis :

1. **PASS** `<password>` → Vérifie le mot de passe serveur

2. **NICK** `<nickname>` → Définit le pseudo (validation de caractères)

3. **USER** `<username> 0 * :realname` → Définit le nom d'utilisateur



### Validation du NICK :

- Premier caractère : lettre ou `[ ] \ ^ _ { } |`

- Caractères suivants : lettre, chiffre, ou `[ ] \ ^ _ { } | -`

- Doit être **unique** sur le serveur



### `tryFinalize()` :

Appelée après NICK et USER. Si `_passOk == true` ET `nick` non vide ET `user` non vide → le client reçoit le **message 001** (RPL_WELCOME) et `_welcomed = true`.



### Le dispatcher bloque les non-enregistrés :

```cpp

if (!sess.isWelcomed()) {

    replyNumeric(sess, "451", ":You have not registered");

    return;

}

```

Seuls `PASS`, `NICK`, `USER` et `QUIT` sont accessibles avant l'enregistrement.



---



## 8. Commandes supportées (détail)



### PASS `<password>`

- Compare avec `_secret`

- Erreur 462 si déjà enregistré

- Erreur 464 si mauvais password



### NICK `<nickname>`

- Erreur 431 si vide

- Erreur 464 si PASS pas encore fait

- Erreur 432 si caractères invalides

- Erreur 433 si pseudo déjà pris

- Si changement post-enregistrement → notifie tous les channels



### USER `<username> 0 * :realname>`

- Erreur 462 si déjà enregistré

- Erreur 464 si PASS pas encore fait



### QUIT `[:reason]`

- Envoie un message QUIT à tous les channels du user

- Déconnecte le client proprement



### JOIN `<#channel> [key]`

- Crée le channel s'il n'existe pas (et le créateur devient opérateur)

- Vérifie : mode +i (invité ?), mode +k (mot de passe ?), mode +l (limite ?)

- Envoie la ligne JOIN à tout le channel

- Envoie le topic (332) et la liste des noms (353 + 366)

- Les ops sont préfixés par `@` dans la liste des noms



### PART `<#channel> [:reason]`

- Vérifie que le channel existe (403) et que le user y est (442)

- Supprime le channel s'il est vide après le départ



### PRIVMSG `<target> :<message>`

- Si target commence par `#` → message au channel (relay à tout le channel sauf l'envoyeur)

- Sinon → message privé à un user spécifique

- Erreur 401 si user introuvable, 403 si channel introuvable, 442 si pas dans le channel



### KICK `<#channel> <user> [:reason]`

- **Opérateur uniquement** (erreur 482 sinon)

- Envoie le KICK à tout le channel puis retire l'user

- Supprime le channel s'il est vide après



### INVITE `<nick> <#channel>`

- **Opérateur uniquement**

- Ajoute le nick à la `_guestList` du channel (pour bypass du mode +i)

- Envoie une notification INVITE au destinataire

- Erreur 443 si déjà dans le channel



### TOPIC `<#channel> [:<new_topic>]`

- Sans argument → affiche le topic actuel (332) ou "No topic" (331)

- Avec argument → change le topic

- Si mode +t actif → **seuls les ops** peuvent changer le topic



### MODE `<#channel> [+/-flags] [params]`

- Sans flags → affiche les modes actuels (324)

- Avec flags → modifie les modes (opérateur uniquement)



### PING `<token>`

- Répond `PONG` (keepalive)



---



## 9. Les modes de channel (MODE)



| Flag | Nom | Effet | Paramètre |

|------|-----|-------|-----------|

| `+i` | Invite-only | Seuls les invités peuvent rejoindre | Aucun |

| `-i` | | Désactive invite-only | Aucun |

| `+t` | Topic lock | Seuls les ops peuvent changer le topic | Aucun |

| `-t` | | Tout le monde peut changer le topic | Aucun |

| `+k` | Key | Met un mot de passe sur le channel | `<password>` |

| `-k` | | Retire le mot de passe | Aucun |

| `+o` | Operator | Donne le statut opérateur à un user | `<nick>` |

| `-o` | | Retire le statut opérateur | `<nick>` |

| `+l` | Limit | Fixe une limite d'utilisateurs | `<nombre>` |

| `-l` | | Retire la limite | Aucun |



### Comment ça marche dans le code :

Le parsing de MODE utilise un `istringstream` pour extraire le target, le mode string (ex: `+itk`), et les arguments restants. Il itère caractère par caractère sur le mode string. Un `+` active le mode adding, un `-` le désactive. Chaque flag valide est appliqué et ajouté à la string `applied`. À la fin, un message MODE est relayé à tout le channel.



---



## 10. Gestion mémoire et signaux



### Mémoire :

- Les `Session*` et `Room*` sont alloués avec `new`

- `dropConnection()` fait `delete` de la Session

- Quand un channel se vide → `delete` de la Room + `erase` de la map

- `shutdown()` nettoie **tout** : parcourt `_sessions` et `_rooms`, delete chaque pointeur, ferme tous les fd



### Signaux :

- `SIGINT` (Ctrl+C), `SIGTERM`, `SIGQUIT` sont interceptés

- Le handler met `g_caught_sig = 1` (variable globale `volatile sig_atomic_t`)

- La boucle `loop()` vérifie `g_caught_sig` à chaque tour et sort proprement

- **Pas de `exit()` dans le handler** (c'est interdit / dangereux)



### Prefix IRC :

Les messages IRC suivent le format `:nick!user@host COMMAND params\r\n`. La fonction `buildPrefix()` construit `:nick!user@localhost`.



### Numeric replies :

Les réponses numériques IRC (`001`, `401`, `461`, etc.) sont envoyées via `replyNumeric()` au format `:ft_irc CODE nick :message`.



---



## 11. Questions pièges fréquentes



### ❓ "Pourquoi poll() et pas select() ?"

> `poll()` n'a pas la limite de 1024 fd de `select()`. Plus propre car on passe un tableau de `pollfd` au lieu de sets de bits. Les deux marchent mais `poll()` est recommandé par le sujet.



### ❓ "Pourquoi des sockets non-bloquants ?"

> Pour ne jamais bloquer le serveur. Si un client est lent, `send()` bloquant bloquerait TOUT le serveur. Avec non-bloquant, on buffer les données et on les envoie quand le fd est prêt (POLLOUT).



### ❓ "Que se passe-t-il si un client envoie des données incomplètes ?"

> Les données sont accumulées dans `_recvBuf`. On ne traite une commande que quand on trouve un `\n`. Si le buffer dépasse 4096 sans `\n` → on déconnecte (protection anti-flood).



### ❓ "Qu'est-ce qu'un opérateur de channel ?"

> Le premier user à rejoindre un channel en devient automatiquement opérateur. Un op peut : KICK, INVITE, changer le TOPIC (si +t), changer les MODEs du channel, donner/retirer le statut op (+o/-o).



### ❓ "Comment gérez-vous les fuites mémoire ?"

> Chaque `new Session` a son `delete` dans `dropConnection()`. Chaque `new Room` a son `delete` quand le channel se vide ou dans `shutdown()`. Les maps (`_sessions`, `_rooms`) sont itérées et vidées dans `shutdown()`.



### ❓ "Que fait `fcntl(fd, F_SETFL, O_NONBLOCK)` ?"

> Met le file descriptor en mode non-bloquant. Les appels `recv()` et `send()` retournent immédiatement au lieu de bloquer le processus.



### ❓ "Pourquoi `SO_REUSEADDR` ?"

> Permet de relancer le serveur immédiatement après un crash sans attendre le timeout TCP (le port reste dans l'état TIME_WAIT pendant ~60s sinon).



### ❓ "Comment testez-vous ?"

> Avec `nc` (netcat) pour des tests manuels, ou avec un vrai client IRC comme `irssi`. On peut ouvrir plusieurs terminaux avec nc pour tester les messages entre clients.



### ❓ "Pourquoi `\r\n` ?"

> Le protocole IRC (RFC 1459) spécifie que chaque message se termine par `\r\n` (CRLF). Le serveur accepte aussi `\n` seul en entrée (il strip le `\r` si présent).



### ❓ "C'est quoi la variable globale `g_caught_sig` ?"

> C'est le seul moyen safe de communiquer entre un signal handler et le code principal. Elle est `volatile sig_atomic_t` pour être lue/écrite de manière atomique. Le handler ne fait que la mettre à 1, et la boucle la check.



### ❓ "Que se passe-t-il quand un user QUIT ?"

> `purgeFromRooms()` parcourt tous les channels, envoie un message QUIT aux autres membres, retire le user. Si un channel se retrouve vide → il est supprimé. Puis le fd est fermé et la Session delete.



---



## 12. Répartition en 2 parties



---



### 🅰️ PARTIE A — Réseau, Serveur & Connexions



**Fichiers à connaître :**

- `main.cpp`

- `IRCCore.cpp`

- `IRCCoreHelpers.cpp`

- `Session.cpp`

- `helpers.cpp`

- `commands/Dispatcher.cpp`

- `commands/Registration.cpp` (PASS, NICK, USER, QUIT)



**Concepts à maîtriser :**

1. **Sockets TCP** : `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()`, `close()`

2. **poll()** : comment ça marche, `POLLIN`, `POLLOUT`, `POLLERR`, `POLLHUP`

3. **Non-bloquant** : `fcntl()`, `O_NONBLOCK`, pourquoi c'est nécessaire

4. **SO_REUSEADDR** : pourquoi on l'utilise

5. **Buffering** : `_recvBuf` (réception), `_outBuf` (envoi), `drainOutBuf()`

6. **Boucle principale** : le flow complet de `loop()`, quand on accepte, quand on lit, quand on envoie

7. **Signaux** : `signal()`, `volatile sig_atomic_t`, arrêt propre

8. **Parsing** : `parseVerb()`, `parseArgs()`, comment une ligne brute devient une commande

9. **Enregistrement** : flow PASS → NICK → USER → `tryFinalize()` → welcome 001

10. **Session** : les attributs, le cycle de vie, `_passOk`, `_welcomed`

11. **Dispatcher** : comment `dispatch()` route les commandes, pourquoi les non-enregistrés sont bloqués

12. **Déconnexion** : `dropConnection()`, `purgeFromRooms()`, nettoyage mémoire



**Questions qu'on peut te poser :**

- Explique la boucle poll()

- Pourquoi non-bloquant ?

- Comment gères-tu un client qui envoie des données partielles ?

- Que se passe-t-il à la réception de Ctrl+C ?

- Comment fonctionne l'enregistrement ?

- Qu'est-ce que SO_REUSEADDR ?



---



### 🅱️ PARTIE B — Channels, Commandes & Administration



**Fichiers à connaître :**

- `Room.hpp` / `Room.cpp`

- `commands/RoomCommands.cpp` (JOIN, PART)

- `commands/Messaging.cpp` (PRIVMSG)

- `commands/AdminCommands.cpp` (KICK, INVITE, TOPIC, MODE)



**Concepts à maîtriser :**

1. **Room** : les attributs (`_users`, `_admins`, `_guestList`, `_restricted`, etc.)

2. **JOIN** : création de channel, premier user = op, vérification des modes (+i, +k, +l), envoi de NAMES

3. **PART** : quitter un channel, suppression si vide

4. **PRIVMSG** : message à un channel (relay) vs message privé (direct), vérifications d'erreur

5. **KICK** : vérification op, retrait de l'user, suppression si vide

6. **INVITE** : ajout à la `_guestList`, notification au destinataire

7. **TOPIC** : lecture vs écriture, interaction avec mode +t

8. **MODE** : les 5 flags (i, t, k, o, l), le parsing caractère par caractère, `+` et `-`, les params

9. **relay() vs relayAll()** : relay = tous sauf un, relayAll = tout le monde

10. **Le préfixe IRC** : format `:nick!user@host COMMANDE params\r\n`

11. **Codes numériques** : 001 (welcome), 332 (topic), 353 (names), 401 (no such nick), 403 (no such channel), 441 (not on channel), 442 (you're not on that channel), 461 (not enough params), 462 (already registered), 464 (password incorrect), 471 (+l full), 473 (+i), 475 (+k), 482 (not op)

12. **Gestion mémoire des Rooms** : quand delete, quand erase de la map



**Questions qu'on peut te poser :**

- Qui devient opérateur quand on crée un channel ?

- Que se passe-t-il si quelqu'un essaie de JOIN un channel +i sans être invité ?

- Comment fonctionne MODE +k ?

- Quelle différence entre relay() et relayAll() ?

- Comment fonctionne PRIVMSG vers un user vs vers un channel ?

- Que se passe-t-il quand le dernier user quitte un channel ?

- Explique le flow complet d'un KICK



---



### 📊 Récapitulatif de la répartition



| | Partie A | Partie B |

|---|----------|----------|

| **Thème** | Réseau & Infrastructure | Channels & Commandes |

| **Fichiers** | main, IRCCore, IRCCoreHelpers, Session, helpers, Dispatcher, Registration | Room, RoomCommands, Messaging, AdminCommands |

| **Lignes de code** | ~550 lignes | ~560 lignes |

| **Difficulté** | Réseau (poll, sockets) | Logique IRC (modes, commandes) |



Les deux parties sont **équilibrées** en volume et en difficulté. La Partie A est plus technique (réseau/système), la Partie B est plus fonctionnelle (logique IRC).



---