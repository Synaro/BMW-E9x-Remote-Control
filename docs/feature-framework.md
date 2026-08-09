# Cadre des fonctionnalités modulaires

## Périmètre actuel

Le catalogue exécutable contient uniquement les trois fonctions dont le
comportement existe dans `TelemetryMonitor` :

| Code de configuration | Comportement |
|---|---|
| `cold_engine_guard` | alerte de régime élevé lorsque le moteur est froid |
| `dpf_regeneration_indicator` | alertes de début et fin de régénération FAP |
| `transmission_overheat_alert` | alerte de température de boîte avec hystérésis |

Chaque fonction est demandée indépendamment avec
`feature.<code>=true|false` et reste désactivée par défaut. Le moniteur produit
uniquement des états et des alertes : il n'envoie aucune commande véhicule.

Les 40 anciennes entrées sans comportement ont été retirées du code et placées
dans [backlog/features.md](backlog/features.md). Une idée du backlog n'est pas
une fonctionnalité disponible.

## Résolution fermée par défaut

Une fonction du catalogue devient effective uniquement si :

1. l'utilisateur l'a activée ;
2. l'implémentation est déclarée disponible par la cible ;
3. la capacité de lecture de l'état véhicule est présente ;
4. les signaux nécessaires sont qualifiés.

Le simulateur fournit ces preuves pour ses trames synthétiques. La cible réelle
ne les fournit pas encore, car aucun signal BMW n'est qualifié.

| État utile aujourd'hui | Signification |
|---|---|
| `disabled_by_user` | option désactivée |
| `not_implemented` | comportement absent de la cible |
| `missing_capabilities` | lecture de l'état indisponible |
| `signals_unqualified` | sources véhicule non qualifiées |
| `simulated` | comportement actif dans le simulateur |
| `available` | toutes les preuves d'une cible réelle sont présentes |

Les autres catégories et statuts du type générique sont du vocabulaire interne
hérité. Ils ne déclarent aucune fonction d'écriture disponible.

## Configuration et persistance

Le masque de fonctions reste un entier fixe de 64 bits afin de conserver un
format embarqué simple. Seuls les trois bits du catalogue courant sont valides ;
tout autre bit est refusé.

La configuration pré-V1 utilise un seul payload de 40 octets. Aucune migration
des anciens formats de développement n'est conservée, puisqu'aucun matériel
déployé ne dépend de ces formats.

Voir [telemetry-alerts.md](telemetry-alerts.md) pour les règles détaillées et
[user-configuration.md](user-configuration.md) pour les limites utilisateur.
