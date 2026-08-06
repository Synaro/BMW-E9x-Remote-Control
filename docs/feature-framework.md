# Cadre des fonctionnalités modulaires

## Objectif

Le projet possède un catalogue stable de 43 fonctionnalités. Chacune peut être
demandée indépendamment par l'utilisateur avec une clé
`feature.<code>=true|false`. Toutes sont désactivées par défaut.

Activer une clé ne suffit jamais à autoriser une action sur le véhicule. Le
moteur de résolution sépare quatre informations :

1. la préférence de l'utilisateur ;
2. l'existence réelle de l'implémentation ;
3. les capacités matérielles et logicielles disponibles ;
4. la qualification des signaux et des commandes pour la variante automobile.

Une fonctionnalité devient effective uniquement si toutes ces conditions sont
réunies. En simulation, elle est explicitement marquée `simulated` ; sur le
véhicule, elle doit atteindre l'état `available`.

## États possibles

| État | Signification |
|---|---|
| `disabled_by_user` | option désactivée dans la configuration |
| `not_implemented` | option demandée mais code fonctionnel absent |
| `missing_capabilities` | matériel, transport ou application compagnon absent |
| `signals_unqualified` | données véhicule non qualifiées pour cette variante |
| `comfort_writes_unqualified` | écriture de confort non autorisée |
| `critical_control_blocked` | commande critique maintenue bloquée |
| `simulated` | comportement disponible uniquement dans le simulateur |
| `available` | toutes les barrières de la cible réelle sont satisfaites |

Ce modèle évite qu'une case cochée transforme une idée future en commande
réelle. Les fonctions dangereuses restent bloquées même si leur préférence est
enregistrée.

## Niveaux de livraison

### V1 lecture seule

La V1 conserve le démarrage distant comme fonction centrale existante et ouvre
le chantier télémétrie/alertes sans écriture supplémentaire sur les calculateurs.
Les candidats du catalogue sont :

- notification d'alarme ;
- shift-light externe ;
- dashboard racing Android ;
- protection visuelle moteur froid ;
- indicateur de régénération FAP ;
- alerte de surchauffe de boîte ;
- enregistreur de vol ;
- OBD2 BLE virtuel.

`v1_read_only` indique une priorité de développement, pas une promesse que le
signal BMW est déjà identifié. La qualification de chaque donnée reste
obligatoire.

La protection moteur froid, l'indicateur de régénération FAP et l'alerte de
surchauffe de boîte possèdent maintenant leur comportement complet dans le
simulateur. Ils restent `unavailable` sur une cible réelle tant que leurs
signaux BMW ne sont pas qualifiés. Voir
[telemetry-alerts.md](telemetry-alerts.md).

### Confort futur

Les automatismes de confort et sorties externes sont classés
`future_comfort`. Ils nécessitent un adaptateur dédié, des règles d'arbitrage et,
pour toute écriture véhicule, une qualification explicite.

### Banc uniquement

Les fonctions touchant au groupe motopropulseur, à l'accès au véhicule ou à une
signalisation agressive sont classées `bench_only`. Cela couvre notamment le
kill-switch, le mode valet, l'anti-carjacking, la régénération FAP forcée, le
turbo timer et les strobes. Elles ne doivent pas être activées sur route par la
simple configuration utilisateur.

## iPhone et Android

Les fonctions génériques liées au téléphone décrivent une application compagnon
comme une capacité alternative : iOS **ou** Android peut satisfaire le besoin.
Le cœur embarqué et le format de configuration ne dépendent donc pas d'une seule
plateforme.

Le dashboard racing demandé pour l'écran Erisin reste naturellement catalogué
comme spécifique Android. L'application iPhone sera prioritaire pour le premier
compagnon mobile ; Android pourra être ajouté plus tard au-dessus du même
protocole. Aucune application mobile n'est encore livrée dans ce jalon.

## Identifiants et stockage

Les identifiants sont ordonnés et persistés dans un masque fixe de 64 bits. Ils
ne doivent jamais être réordonnés ni réutilisés après publication. Le catalogue
compte actuellement 43 entrées, laissant 21 emplacements compatibles sans
allocation dynamique.

La configuration binaire V2 a ajouté ce masque au payload. La V3 ajoute les
seuils de télémétrie. Les configurations V1 de 24 octets et V2 de 32 octets
restent acceptées ; les champs absents prennent leurs valeurs sûres par défaut.
La sauvegarde suivante les écrit au format V3.

## Inspection

Après compilation du simulateur, le catalogue complet et ses classifications
peuvent être affichés avec :

```powershell
.\build\bmw_remote_simulator.exe --list-features
```

Les options actives d'un fichier peuvent être vérifiées avec :

```powershell
.\build\bmw_remote_simulator.exe `
  --show-config .\config\user-settings.conf
```

Le fichier d'exemple contient les 43 clés avec la valeur `false`.
