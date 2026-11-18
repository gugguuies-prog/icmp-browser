# Requirements Document

## Introduction

Ce document spécifie les exigences pour un navigateur web portable Windows qui utilise le tunneling ICMP pour communiquer avec un serveur proxy, permettant ainsi de contourner les portails captifs et les restrictions réseau sans nécessiter de droits administrateur.

Le système s'inspire du projet icmptunnel existant mais adapte l'approche pour Windows en utilisant l'API native `Icmp.dll` (IcmpSendEcho) au lieu de raw sockets POSIX, et intègre un moteur de rendu web dans une application portable.

## Glossary

- **ICMP_Browser_Client**: L'application navigateur portable Windows qui envoie le trafic HTTP via ICMP
- **ICMP_Proxy_Server**: Le serveur proxy qui reçoit les paquets ICMP, extrait le trafic IP, et le transmet sur Internet
- **Icmp_dll**: L'API Windows native (`Icmp.dll`) fournissant IcmpSendEcho et IcmpCreateFile
- **ICMP_Packet**: Un paquet ICMP Echo Request (type 8) ou Echo Reply (type 0) contenant du trafic IP encapsulé
- **WebView2**: Le moteur de rendu web Microsoft Edge Chromium intégrable dans les applications Windows
- **Payload**: La charge utile d'un paquet ICMP contenant les données IP encapsulées
- **MTU**: Maximum Transmission Unit - taille maximale d'un paquet (1472 bytes pour ICMP)
- **HTTP_Proxy_Engine**: Le composant qui intercepte les requêtes HTTP du navigateur et les encapsule dans ICMP
- **Decapsulator**: Le composant serveur qui extrait le trafic IP des paquets ICMP
- **Encapsulator**: Le composant client qui encapsule le trafic IP dans des paquets ICMP
- **Standard_User_Account**: Un compte utilisateur Windows sans privilèges administrateur

## Requirements

### Requirement 1: Encapsulation ICMP du trafic HTTP

**User Story:** En tant qu'utilisateur sans droits administrateur, je veux que mes requêtes HTTP soient automatiquement encapsulées dans des paquets ICMP, afin de contourner les restrictions réseau sans configuration complexe.

#### Acceptance Criteria

1. WHEN THE ICMP_Browser_Client génère une requête HTTP, THE Encapsulator SHALL encapsuler le paquet IP dans la payload d'un paquet ICMP Echo Request (type 8)
2. THE Encapsulator SHALL limiter la taille de la payload à 1472 bytes conformément au MTU défini
3. WHEN un paquet IP dépasse 1472 bytes, THE Encapsulator SHALL fragmenter le paquet en plusieurs paquets ICMP
4. THE Encapsulator SHALL calculer et définir le checksum ICMP pour chaque paquet envoyé
5. THE Encapsulator SHALL définir un identifiant unique pour chaque session de tunnel dans le champ ID du header ICMP
6. THE Encapsulator SHALL incrémenter le numéro de séquence pour chaque paquet ICMP envoyé dans une session

### Requirement 2: Utilisation de l'API Windows Icmp.dll

**User Story:** En tant qu'utilisateur avec un compte standard, je veux que le navigateur fonctionne sans privilèges administrateur, afin de pouvoir l'utiliser sur des postes restreints.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL utiliser IcmpCreateFile de Icmp_dll pour obtenir un handle ICMP
2. THE ICMP_Browser_Client SHALL utiliser IcmpSendEcho de Icmp_dll pour envoyer des paquets ICMP Echo Request
3. THE ICMP_Browser_Client SHALL utiliser IcmpSendEcho2 de Icmp_dll pour recevoir des paquets ICMP Echo Reply de manière asynchrone
4. THE ICMP_Browser_Client SHALL utiliser IcmpCloseHandle de Icmp_dll pour fermer le handle ICMP lors de la terminaison
5. THE ICMP_Browser_Client SHALL fonctionner avec un Standard_User_Account sans requérir UAC ou privilèges élevés
6. IF IcmpCreateFile retourne INVALID_HANDLE_VALUE, THEN THE ICMP_Browser_Client SHALL afficher un message d'erreur explicite et terminer proprement

### Requirement 3: Communication client-serveur bidirectionnelle complète

**User Story:** En tant qu'utilisateur, je veux que le navigateur envoie et reçoive des données via ICMP, afin d'établir une communication bidirectionnelle complète avec Internet dans les deux sens (sortant ET entrant).

#### Acceptance Criteria

1. WHEN THE ICMP_Browser_Client envoie un paquet ICMP Echo Request, THE ICMP_Proxy_Server SHALL recevoir et décapsuler le trafic IP
2. THE ICMP_Proxy_Server SHALL transmettre le trafic IP décapsulé vers la destination Internet appropriée (forward tunnel)
3. WHEN THE ICMP_Proxy_Server reçoit une réponse IP d'Internet, THE Encapsulator SHALL encapsuler la réponse dans un paquet ICMP Echo Reply (type 0)
4. THE ICMP_Proxy_Server SHALL envoyer le paquet ICMP Echo Reply vers THE ICMP_Browser_Client
5. WHEN THE ICMP_Browser_Client reçoit un paquet ICMP Echo Reply, THE Decapsulator SHALL extraire le trafic IP de la payload
6. THE ICMP_Browser_Client SHALL transmettre le trafic IP décapsulé au moteur de rendu web
7. WHEN THE ICMP_Proxy_Server reçoit une connexion entrante depuis Internet destinée au client, THE ICMP_Proxy_Server SHALL encapsuler le paquet dans ICMP Echo Request et l'envoyer au client (reverse tunnel)
8. THE ICMP_Browser_Client SHALL écouter et accepter les connexions entrantes relayées par le serveur proxy
9. THE ICMP_Browser_Client SHALL exposer un port de service local (ex: 8080) accessible via le tunnel reverse
10. THE ICMP_Proxy_Server SHALL maintenir une table de mapping (port public → client + port local) pour router les connexions entrantes

### Requirement 4: Intégration du moteur de rendu web

**User Story:** En tant qu'utilisateur, je veux naviguer sur des sites web avec une interface graphique moderne, afin d'avoir une expérience utilisateur comparable à un navigateur standard.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL intégrer WebView2 comme moteur de rendu web
2. THE ICMP_Browser_Client SHALL afficher une barre d'adresse permettant la saisie d'URLs
3. THE ICMP_Browser_Client SHALL afficher des boutons de navigation (précédent, suivant, actualiser, accueil)
4. WHEN l'utilisateur saisit une URL, THE ICMP_Browser_Client SHALL router le trafic HTTP via THE HTTP_Proxy_Engine
5. THE HTTP_Proxy_Engine SHALL intercepter toutes les requêtes HTTP/HTTPS générées par WebView2
6. THE ICMP_Browser_Client SHALL afficher le statut de connexion (connecté au proxy, tunnel actif, erreur)
7. THE ICMP_Browser_Client SHALL supporter JavaScript, CSS, et HTML5 via WebView2

### Requirement 5: Portabilité de l'application

**User Story:** En tant qu'utilisateur mobile, je veux un exécutable unique sans installation, afin de pouvoir utiliser le navigateur depuis une clé USB sur différents ordinateurs.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL être distribué comme un seul fichier exécutable portable (.exe)
2. THE ICMP_Browser_Client SHALL embarquer toutes les dépendances nécessaires (sauf WebView2 Runtime)
3. THE ICMP_Browser_Client SHALL détecter la présence de WebView2 Runtime au démarrage
4. IF WebView2 Runtime n'est pas installé, THEN THE ICMP_Browser_Client SHALL proposer de télécharger la version Evergreen Bootstrapper
5. THE ICMP_Browser_Client SHALL stocker la configuration dans le même répertoire que l'exécutable
6. THE ICMP_Browser_Client SHALL créer un fichier de configuration par défaut s'il n'existe pas au premier lancement
7. THE ICMP_Browser_Client SHALL fonctionner sans écrire dans le registre Windows
8. THE ICMP_Browser_Client SHALL fonctionner sans écrire dans les répertoires système Windows

### Requirement 6: Configuration du serveur proxy

**User Story:** En tant qu'utilisateur, je veux configurer facilement l'adresse IP du serveur proxy, afin de pouvoir changer de serveur selon mes besoins.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL lire l'adresse IP du serveur proxy depuis un fichier de configuration (config.ini)
2. THE ICMP_Browser_Client SHALL afficher l'adresse du serveur proxy dans l'interface utilisateur
3. THE ICMP_Browser_Client SHALL permettre la modification de l'adresse du serveur proxy via un menu Paramètres
4. WHEN l'utilisateur modifie l'adresse du serveur proxy, THE ICMP_Browser_Client SHALL valider le format d'adresse IPv4
5. IF l'adresse IPv4 est invalide, THEN THE ICMP_Browser_Client SHALL afficher un message d'erreur et conserver l'ancienne valeur
6. THE ICMP_Browser_Client SHALL sauvegarder automatiquement les modifications dans config.ini
7. THE ICMP_Browser_Client SHALL permettre de tester la connectivité avec le serveur proxy (ping ICMP)

### Requirement 7: Gestion des erreurs et résilience

**User Story:** En tant qu'utilisateur, je veux que le navigateur gère les erreurs réseau de manière robuste, afin de maintenir une expérience stable même en cas de problèmes de connexion.

#### Acceptance Criteria

1. WHEN le serveur proxy ne répond pas pendant 5 secondes, THE ICMP_Browser_Client SHALL afficher un message d'erreur de timeout
2. WHEN un paquet ICMP est perdu, THE ICMP_Browser_Client SHALL réessayer l'envoi jusqu'à 3 fois avec un délai exponentiel
3. IF les 3 tentatives échouent, THEN THE ICMP_Browser_Client SHALL notifier l'utilisateur d'une erreur de connexion
4. THE ICMP_Browser_Client SHALL maintenir une file d'attente des paquets en cours d'envoi
5. THE ICMP_Browser_Client SHALL implémenter un mécanisme d'acquittement pour confirmer la réception des paquets
6. WHEN un paquet fragmenté arrive incomplet après 10 secondes, THE ICMP_Browser_Client SHALL supprimer les fragments partiels
7. THE ICMP_Browser_Client SHALL logger toutes les erreurs réseau dans un fichier de log local

### Requirement 8: Sécurité et chiffrement des communications

**User Story:** En tant qu'utilisateur soucieux de la sécurité, je veux que mes données soient chiffrées dans le tunnel ICMP, afin de protéger ma vie privée contre les interceptions.

#### Acceptance Criteria

1. THE Encapsulator SHALL chiffrer la payload IP avant encapsulation dans les paquets ICMP en utilisant AES-256-GCM
2. THE ICMP_Browser_Client SHALL générer une clé de session unique pour chaque session de tunnel
3. THE ICMP_Browser_Client SHALL échanger la clé de session avec THE ICMP_Proxy_Server via un handshake Diffie-Hellman
4. THE Decapsulator SHALL déchiffrer la payload ICMP après réception et avant extraction du trafic IP
5. IF le déchiffrement échoue, THEN THE ICMP_Browser_Client SHALL rejeter le paquet et incrémenter un compteur d'erreurs
6. THE ICMP_Browser_Client SHALL supporter HTTPS de bout en bout pour les sites web accédés
7. THE ICMP_Browser_Client SHALL valider les certificats SSL/TLS des sites web via WebView2

### Requirement 9: Gestion de la bande passante et performance

**User Story:** En tant qu'utilisateur sur une connexion limitée, je veux que le navigateur optimise l'utilisation de la bande passante, afin de naviguer de manière fluide malgré les limitations du protocole ICMP.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL limiter le débit d'envoi de paquets ICMP à 100 paquets par seconde par défaut
2. THE ICMP_Browser_Client SHALL permettre la configuration du débit maximal dans les paramètres
3. THE ICMP_Browser_Client SHALL implémenter un système de file d'attente avec priorité pour les paquets
4. THE ICMP_Browser_Client SHALL prioriser les paquets DNS et les premiers paquets HTTP d'une connexion
5. WHEN la file d'attente dépasse 1000 paquets, THE ICMP_Browser_Client SHALL supprimer les paquets les plus anciens
6. THE ICMP_Browser_Client SHALL afficher des statistiques de bande passante (débit montant/descendant, paquets perdus)
7. THE ICMP_Browser_Client SHALL compresser les payloads IP avec zlib avant chiffrement pour réduire la taille

### Requirement 10: Développement du serveur proxy ICMP

**User Story:** En tant qu'administrateur du serveur proxy, je veux un serveur Linux capable de recevoir et traiter les paquets ICMP du client, afin de fournir l'accès Internet tunnelisé.

#### Acceptance Criteria

1. THE ICMP_Proxy_Server SHALL être développé en C pour les systèmes Linux/UNIX
2. THE ICMP_Proxy_Server SHALL utiliser des raw sockets (IPPROTO_ICMP) pour recevoir les paquets ICMP
3. THE ICMP_Proxy_Server SHALL requérir les privilèges root pour créer des raw sockets
4. WHEN THE ICMP_Proxy_Server reçoit un paquet ICMP Echo Request, THE Decapsulator SHALL extraire la payload IP chiffrée
5. THE ICMP_Proxy_Server SHALL déchiffrer la payload avec la clé de session établie
6. THE ICMP_Proxy_Server SHALL injecter le trafic IP décapsulé dans une interface TUN virtuelle
7. THE ICMP_Proxy_Server SHALL configurer le NAT/masquerading via iptables pour router le trafic vers Internet
8. WHEN THE ICMP_Proxy_Server reçoit une réponse IP depuis Internet, THE Encapsulator SHALL la chiffrer et l'encapsuler dans un ICMP Echo Reply
9. THE ICMP_Proxy_Server SHALL envoyer l'ICMP Echo Reply vers l'adresse IP source du client
10. THE ICMP_Proxy_Server SHALL supporter plusieurs clients simultanés en maintenant des sessions séparées par adresse IP et ID ICMP

### Requirement 11: Parseur et formateur de paquets ICMP

**User Story:** En tant que développeur, je veux des composants fiables pour parser et formater les paquets ICMP, afin d'assurer l'intégrité de la communication.

#### Acceptance Criteria

1. WHEN un paquet ICMP brut est reçu, THE ICMP_Parser SHALL extraire le type, code, checksum, ID, et numéro de séquence
2. WHEN un paquet ICMP brut est reçu, THE ICMP_Parser SHALL extraire la payload et sa taille
3. THE ICMP_Parser SHALL valider le checksum du paquet ICMP reçu
4. IF le checksum est invalide, THEN THE ICMP_Parser SHALL retourner une erreur de validation
5. THE ICMP_Formatter SHALL construire un paquet ICMP valide à partir d'un type, code, ID, séquence, et payload
6. THE ICMP_Formatter SHALL calculer automatiquement le checksum lors de la construction du paquet
7. THE Pretty_Printer SHALL formater les structures ICMP_Packet en représentation hexadécimale lisible
8. FOR ALL paquets ICMP valides, parser puis formater puis parser SHALL produire une structure équivalente (round-trip property)

### Requirement 12: Interface utilisateur et expérience utilisateur

**User Story:** En tant qu'utilisateur non technique, je veux une interface intuitive, afin de pouvoir utiliser le navigateur sans connaissances réseau avancées.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL afficher une fenêtre principale avec une barre d'outils en haut et une zone de contenu web
2. THE ICMP_Browser_Client SHALL utiliser des icônes standards pour les boutons de navigation
3. THE ICMP_Browser_Client SHALL afficher un indicateur de chargement pendant les requêtes HTTP
4. WHEN le tunnel ICMP est actif, THE ICMP_Browser_Client SHALL afficher une icône verte dans la barre d'état
5. WHEN le tunnel ICMP est inactif ou en erreur, THE ICMP_Browser_Client SHALL afficher une icône rouge dans la barre d'état
6. THE ICMP_Browser_Client SHALL afficher une info-bulle explicative au survol de l'indicateur de statut
7. THE ICMP_Browser_Client SHALL permettre l'accès au menu Paramètres via un bouton ou raccourci clavier (Ctrl+,)
8. THE ICMP_Browser_Client SHALL afficher une page d'accueil par défaut lors du premier lancement avec des instructions de configuration

### Requirement 13: Logs et diagnostics

**User Story:** En tant qu'utilisateur rencontrant des problèmes, je veux accéder à des logs détaillés, afin de diagnostiquer les erreurs de connexion.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL créer un fichier de log (icmp_browser.log) dans le répertoire de l'application
2. THE ICMP_Browser_Client SHALL logger chaque envoi de paquet ICMP avec timestamp, taille, et adresse de destination
3. THE ICMP_Browser_Client SHALL logger chaque réception de paquet ICMP avec timestamp, taille, et adresse source
4. THE ICMP_Browser_Client SHALL logger toutes les erreurs avec niveau de sévérité (INFO, WARNING, ERROR)
5. THE ICMP_Browser_Client SHALL limiter la taille du fichier de log à 10 MB
6. WHEN le fichier de log dépasse 10 MB, THE ICMP_Browser_Client SHALL créer un fichier de rotation et conserver les 3 derniers fichiers
7. THE ICMP_Browser_Client SHALL fournir un menu pour afficher les logs dans une fenêtre dédiée
8. THE ICMP_Browser_Client SHALL permettre l'exportation des logs vers un fichier texte

### Requirement 14: Compatibilité et prérequis système

**User Story:** En tant qu'utilisateur Windows, je veux connaître les prérequis système, afin de m'assurer que le navigateur fonctionnera sur ma machine.

#### Acceptance Criteria

1. THE ICMP_Browser_Client SHALL fonctionner sur Windows 10 version 1809 et supérieure
2. THE ICMP_Browser_Client SHALL fonctionner sur Windows 11 toutes versions
3. THE ICMP_Browser_Client SHALL supporter les architectures x64 et ARM64
4. THE ICMP_Browser_Client SHALL requérir WebView2 Runtime (Evergreen) installé ou disponible
5. THE ICMP_Browser_Client SHALL requérir au minimum 100 MB d'espace disque disponible
6. THE ICMP_Browser_Client SHALL requérir au minimum 512 MB de RAM disponible
7. THE ICMP_Browser_Client SHALL vérifier la disponibilité de Icmp.dll au démarrage
8. IF Icmp.dll n'est pas disponible, THEN THE ICMP_Browser_Client SHALL afficher un message d'erreur et terminer

### Requirement 15: Gestion des sessions et reconnexion

**User Story:** En tant qu'utilisateur mobile, je veux que le navigateur maintienne la session lors de changements de réseau, afin de ne pas perdre mes pages ouvertes.

#### Acceptance Criteria

1. WHEN la connexion réseau change, THE ICMP_Browser_Client SHALL détecter la perte de connectivité avec le serveur proxy
2. THE ICMP_Browser_Client SHALL tenter automatiquement de rétablir la connexion toutes les 5 secondes
3. THE ICMP_Browser_Client SHALL maintenir l'état des onglets ouverts pendant les tentatives de reconnexion
4. WHEN la reconnexion réussit, THE ICMP_Browser_Client SHALL négocier une nouvelle clé de session
5. THE ICMP_Browser_Client SHALL afficher un message "Reconnexion en cours..." dans la barre d'état
6. THE ICMP_Browser_Client SHALL permettre à l'utilisateur d'annuler les tentatives de reconnexion
7. IF la reconnexion échoue après 10 tentatives, THEN THE ICMP_Browser_Client SHALL afficher un message invitant à vérifier la configuration
