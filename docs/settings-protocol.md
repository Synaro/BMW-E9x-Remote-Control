# Protocole de configuration du boîtier

## Objectif

Le protocole relie le configurateur PC au futur boîtier sans dépendre d'un port
série, de Bluetooth ou d'une interface web. Il transporte uniquement les
préférences `UserSettings`. Il ne transporte aucune commande moteur, aucun ordre
d'actionneur et aucun contournement d'antidémarrage.

L'implémentation utilise des tableaux de taille fixe, n'alloue pas dynamiquement
et limite une trame à 40 octets.

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
| 10–11 | 2 | taille du payload, de 0 à 24 |
| 12… | 0–24 | payload |
| fin−4…fin−1 | 4 | CRC‑32 du header et du payload |

Le CRC utilise le polynôme reflété `0xEDB88320`, une valeur initiale et un XOR
final à `0xFFFFFFFF`. Il détecte une corruption accidentelle mais ne constitue
pas une protection cryptographique.

## Messages

| Valeur | Message | Payload |
|---:|---|---:|
| `0x01` | lecture des réglages | 0 |
| `0x02` | écriture des réglages | 24 |
| `0x81` | réponse de lecture | 24 si succès |
| `0x82` | réponse d'écriture | 0 |
| `0xFF` | message non pris en charge | 0 |

L'identifiant de requête est renvoyé tel quel dans la réponse pour associer un
échange. Il n'est ni un secret ni une protection anti-rejeu.

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

Le décodage ne suffit pas à accepter ces valeurs :
`validateUserSettings()` doit aussi accepter l'ensemble.

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
- une écriture est acceptée uniquement lorsque le contrôleur est en `Idle` ;
- la lecture reste possible pendant une session active, sans mutation ;
- un succès d'écriture signifie « enregistré et vérifié » ;
- les nouveaux réglages prennent effet au prochain démarrage du boîtier ;
- une écriture répétée doit être bornée par le futur adaptateur pour protéger la
  mémoire non volatile ;
- le transport doit fournir confidentialité, authentification et anti-rejeu si
  son exposition le nécessite.

Le choix USB, Bluetooth ou réseau reste volontairement ouvert. Aucun adaptateur
ne doit positionner `authorized=true` par défaut ou déduire cette autorisation du
seul CRC.
