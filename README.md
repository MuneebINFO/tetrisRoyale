# Tetris Royale
## Description du projet

Tetris Royale est un jeu Tetris multijoueur en architecture client-serveur, développé en C++ dans le cadre du projet annuel de BA2 en informatique à l’ULB. Cette version constitue toutefois une amélioration personnelle du projet initial.

Le projet implémente une version réseau du célèbre jeu Tetris, permettant à plusieurs joueurs de :

- Se connecter à un serveur central

- Discuter via un chat intégré

- Gérer un système d’amis et d’invitations

- Rejoindre un lobby

- Lancer des parties en mode compétitif

- Jouer avec une logique de jeu gérée côté serveur

Ce projet représente une première implémentation complète d’un système distribué combinant :

- Programmation réseau (sockets)

- Architecture client-serveur

- Gestion d’état côté serveur

- Interface terminal (ncurses)

- Interface en GUI

---

## Installer les dépendances
Pour faire fonctionner le projet, vous aurez besoin de ces dépendances. \
[PostgreSQL](https://www.postgresql.org/download/linux/ubuntu/) · `apt install postgresql` \
[libpqxx](https://pqxx.org/libpqxx/#finding-everything) · `sudo apt install libpqxx-dev`\
[Ncurses](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/intro.html) · `sudo apt install ncurses-dev`

### Compiler le projet
À partir du dossier /build du projet:

```
make
```

## Exécution du logiciel
### Serveur
Une fois la compilation finie, il faut d'abord lancer le serveur. 
```
./tetris_server
```

Celui-ci se met à écouter les connections entrantes a l'adresse `http://localhost:8080`

### Client
Une fois le serveur lancé, vous pouvez lancer le client. \
```
./tetris
```

## Installation de la BDD locale
Vous devez aussi mettre en place votre propre base de données.
### Étape 1 : Installer PostgreSQL (si ce n'est pas fait)
```
sudo apt update
sudo apt install postgresql postgresql-contrib libpqxx-dev
```

### Étape 2 : Démarrer PostgreSQL 
```
sudo service postgresql start
```

### Étape 3 : Création d'un utilisateur et la base game_server
#### Étape 3.1
Lancer PostgreSQL:
```
sudo -u postgres psql
```
#### Étape 3.2
Dans le shell PostgreSQL qui s’ouvre, exécuter les commandes suivantes pour créer un nouvel utilisateur et une base associée:
```
CREATE USER simon WITH PASSWORD 'tetris';
CREATE DATABASE game_server OWNER tetris;
GRANT ALL PRIVILEGES ON DATABASE game_server TO simon;

-- Pour quitter
\q
```
#### Étape 3.3
Dans le terminal (hors de psql), se connecter à la base:
```
psql -U tetris -d game_server
```
#### Étape 3.4
Exécuter les requêtes suivantes:
```
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL,
    player_id INT UNIQUE NOT NULL
);

CREATE TABLE game_invitation (
    id SERIAL PRIMARY KEY,
    id_player_inviting INT NOT NULL,
    id_player_invited INT NOT NULL,
    room_id INT NOT NULL,
    as_gamer BOOLEAN NOT NULL DEFAULT FALSE,
    game_mode VARCHAR(50) NOT NULL DEFAULT 'multiplayer',
    FOREIGN KEY (id_player_inviting) REFERENCES users(player_id) ON DELETE CASCADE,
    FOREIGN KEY (id_player_invited) REFERENCES users(player_id) ON DELETE CASCADE
);

CREATE TABLE friend_request (
    id SERIAL PRIMARY KEY,
    id_player_inviting INT NOT NULL,
    id_player_invited INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_player_inviting) REFERENCES users(player_id) ON DELETE CASCADE,
    FOREIGN KEY (id_player_invited) REFERENCES users(player_id) ON DELETE CASCADE
);

CREATE TABLE friend_list (
    id SERIAL PRIMARY KEY,
    player_id INT NOT NULL,
    friend_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (player_id) REFERENCES users(player_id) ON DELETE CASCADE,
    FOREIGN KEY (friend_id) REFERENCES users(player_id) ON DELETE CASCADE,
    UNIQUE (player_id, friend_id)
);

CREATE TABLE chat_messages (
    id SERIAL PRIMARY KEY,
    sender_id INT NOT NULL,
    receiver_id INT NOT NULL,
    message TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (sender_id) REFERENCES users(player_id) ON DELETE CASCADE,
    FOREIGN KEY (receiver_id) REFERENCES users(player_id) ON DELETE CASCADE
);

CREATE TABLE score (
    username TEXT,
    id SERIAL PRIMARY KEY,	
    high_score INTEGER DEFAULT 0
);
```