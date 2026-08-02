# Commande par clé et session distante

## Cible du produit

Le produit final est un firmware exécuté dans un boîtier embarqué installé dans
le véhicule. Le simulateur Windows n'est qu'un outil de validation du même cœur
applicatif ; il n'est pas destiné à rester dans la voiture.

Le déclenchement prévu utilise trois appuis rapprochés sur le bouton de
verrouillage de la clé d'origine. Le noyau ne connaît ni identifiant CAN BMW ni
broche : un futur adaptateur véhicule devra transformer chaque commande de
verrouillage vérifiée en impulsion sémantique.

## Détection des trois verrouillages

`LockSequenceDetector` est déterministe, sans allocation dynamique et
configurable. Ses valeurs de développement sont :

| Paramètre | Valeur |
|---|---:|
| Nombre d'appuis | 3 |
| Intervalle minimal entre appuis | 80 ms |
| Intervalle maximal entre deux appuis | 1,5 s |
| Durée maximale de la séquence | 3 s |

Un appui trop rapproché est traité comme un rebond. Une séquence trop lente est
abandonnée et le dernier appui devient le début d'une nouvelle séquence. Quand
le troisième appui est reconnu, l'adaptateur peut produire
`RemoteStartRequested`.

Ces paramètres ne sont pas encore qualifiés sur BMW. L'adaptateur réel devra
prouver que l'impulsion vient bien de la chaîne de verrouillage légitime du
véhicule et non d'une trame arbitraire ou rejouée.

## Cycle après démarrage distant

La session applique les règles suivantes :

1. Après confirmation du régime moteur, l'état passe à `Running` et arme une
   limite de 15 minutes.
2. Si aucune portière n'est ouverte avant l'échéance, le moteur passe en
   procédure d'arrêt.
3. Une portière ouverte remplace la limite de 15 minutes par une fenêtre de
   reprise de 60 secondes et passe à `AwaitingTakeover`.
4. L'ouverture seule ne valide jamais la reprise. Sans
   `DriverTakeoverConfirmed` avant l'échéance, le moteur est arrêté.
5. Une reprise authentifiée passe à `DriverControl`, annule le timer distant et
   ordonne `ReleaseRemoteControl` à l'adaptateur d'actionneurs.
6. En `DriverControl`, une commande distante d'arrêt est ignorée. Le contrôleur
   revient à `Idle` quand un régime nul est observé.

`DriverTakeoverConfirmed` est volontairement abstrait. Les preuves exactes
(clé reconnue, séquence de commande, frein, état CAS ou autre source légitime)
seront définies et validées avec le futur adaptateur BMW. Tant qu'elles ne le
sont pas, aucun firmware réel ne doit produire cet événement.

## Paramètres applicatifs

Les deux échéances se règlent dans `ControllerConfig` :

- `maximumRemoteRunTimeMs`, 15 minutes par défaut ;
- `driverTakeoverTimeoutMs`, 60 secondes par défaut.

La configuration de production devra conserver des bornes fixes et être
qualifiée sur banc. Une ouverture de portière ou une notification ne doit jamais
permettre un fonctionnement illimité.
