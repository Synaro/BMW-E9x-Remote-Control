# Protocole de configuration du boîtier

## Objectif

Le protocole relie le configurateur PC au boîtier sans dépendre des détails du
transport. La V1 l'utilise sur le port série USB natif de l'ESP32-S3. Il transporte uniquement les
préférences `UserSettings`. Il ne transporte aucune commande moteur, aucun ordre
d'actionneur et aucun contournement d'antidémarrage.

L'implémentation utilise des tableaux de taille fixe, n'alloue pas dynamiquement
et limite une trame à 48 octets.

## Trame version 1

Tous les entiers multi-octets sont encodés en little-endian.

| Octets | Taille | Champ |
|---:|---:|---|
| 0–3 | 4 | magie ASCII `BMCF` |
| 4 | 1 | version, actuellement `1` |
| 5 | 1 | type de message |
| 6 | 1 | statut, `0` dans une requête valide |
| 7 | 1 | réservé, obligatoirement `0` |
| 8–9 | 2 | identifiant de requête |
| 10–11 | 2 | taille du payload, de 0 à 32 |
| 12… | 0–32 | payload |
| fin−4…fin−1 | 4 | CRC‑32 du header et du payload |

Le CRC utilise le polynôme reflété `0xEDB88320`, une valeur initiale et un XOR
final à `0xFFFFFFFF`. Il détecte une corruption accidentelle mais ne constitue
pas une protection cryptographique.

## Messages

| Valeur | Message | Payload |
|---:|---|---:|
| `0x01` | lecture des réglages | 0 |
| `0x02` | écriture des réglages | 32, ou 24 pour migration V1 |
| `0x03` | identification du boîtier | 0 |
| `0x81` | réponse de lecture | 32 si succès |
| `0x82` | réponse d'écriture | 0 |
| `0x83` | réponse d'identification | 12 si succès |
| `0xFF` | message non pris en charge | 0 |

L'identifiant de requête est renvoyé tel quel dans la réponse pour associer un
échange. Il n'est ni un secret ni une protection anti-rejeu.

## Identité du boîtier

La réponse d'identification permet de confirmer un produit compatible avant
toute écriture :

| Octets | Champ |
|---:|---|
| 0–3 | signature produit ASCII `E9RC` |
| 4 | cible matérielle |
| 5 | version majeure du firmware |
| 6 | version mineure du firmware |
| 7 | correctif du firmware |
| 8–11 | masque de capacités |

La cible `1` désigne l'ESP32-S3-DevKitC-1 et `254` la simulation hôte. Les trois
premiers bits de capacités annoncent respectivement lecture des réglages,
écriture et persistance. Le configurateur refuse une écriture si la signature,
la réponse ou les capacités attendues sont absentes.

## Payload `UserSettings`

Le payload fixe est partagé par le journal persistant et le protocole :

| Octets | Champ |
|---:|---|
| 0 | démarrage distant activé, `0` ou `1` |
| 1 | mode du capot |
| 2 | stratégie d'entrée conducteur |
| 3 | nombre d'appuis sur verrouillage |
| 4–7 | durée maximale moteur en millisecondes |
| 8–11 | délai de reprise en millisecondes |
| 12–15 | intervalle minimal des appuis en millisecondes |
| 16–19 | intervalle maximal des appuis en millisecondes |
| 20–23 | fenêtre totale des appuis en millisecondes |
| 24–31 | masque 64 bits des fonctionnalités modulaires |

Le décodage ne suffit pas à accepter ces valeurs :
`validateUserSettings()` doit aussi accepter l'ensemble.

Le décodeur accepte encore l'ancien payload de 24 octets. Les fonctionnalités
ajoutées en V2 sont alors toutes désactivées. Les lectures et nouvelles
écritures utilisent toujours le payload de 32 octets.

## Statuts applicatifs

| Valeur | Statut | Signification |
|---:|---|---|
| 0 | `ok` | opération réussie |
| 1 | `invalid_payload` | type, statut ou taille incohérent |
| 2 | `unsupported_message` | commande inconnue ou réponse reçue comme requête |
| 3 | `unauthorized` | session non authentifiée |
| 4 | `busy` | écriture refusée car le contrôleur n'est pas en `Idle` |
| 5 | `invalid_settings` | préférences décodées mais rejetées |
| 6 | `storage_failure` | journal non enregistré ou non vérifié |
| 7 | `settings_unavailable` | aucune génération valide à lire |

Les erreurs de structure de trame sont rejetées avant le service : magie,
version, champ réservé, longueur et CRC ne sont donc jamais interprétés comme une
requête applicative.

## Règles de sécurité

- lecture et écriture exigent une session authentifiée par l'adaptateur ;
- le configurateur identifie le produit et ses capacités avant toute écriture ;
- une écriture est acceptée uniquement lorsque le contrôleur est en `Idle` ;
- la lecture reste possible pendant une session active, sans mutation ;
- un succès d'écriture signifie « enregistré et vérifié » ;
- les nouveaux réglages prennent effet au prochain démarrage du boîtier ;
- une écriture répétée doit être bornée par le futur adaptateur pour protéger la
  mémoire non volatile ;
- le transport doit fournir confidentialité, authentification et anti-rejeu si
  son exposition le nécessite.

Sur le prototype de banc, l'accès physique au connecteur USB dédié constitue
l'autorisation locale et le firmware reste figé dans l'état `Idle`. Cette règle
est limitée à cette cible sans adaptateur véhicule. Toute exposition sans accès
physique contrôlé devra ajouter une authentification et une protection anti-rejeu.
Le CRC seul ne fournit aucune de ces propriétés.

## Réception en flux

`SettingsStreamReceiver` accepte les octets un par un. Avant synchronisation, il
ignore le bruit et recherche la séquence `BMCF`. Après le header, il refuse
immédiatement un payload supérieur à 32 octets. Une trame complète passe ensuite
par toutes les vérifications du codec.

Le délai inter-octets est de 250 ms par défaut et reste configurable par
`SettingsStreamConfig`. `poll()` doit être appelé même lorsque le transport est
silencieux afin de libérer une trame partielle. Le calcul d'échéance reste valide
lors du retour à zéro d'un compteur monotone 32 bits.

`SettingsProtocolEndpoint` assemble le récepteur, le service et
`SettingsTransportPort`. Une trame invalide ou expirée ne produit aucune réponse.
Une requête valide produit au plus une réponse de 48 octets. Une panne d'envoi
est signalée à l'adaptateur et ne déclenche aucune boucle de réessai dans le
noyau.
