# Configurateur utilisateur

## Rôle actuel

`bmw_remote_configurator.exe` est un outil PC hors véhicule. Il permet de créer
et modifier les préférences sans connaître la syntaxe du fichier `key=value`.
Il utilise le même `UserSettings` et le même validateur que le contrôleur.

Il ne communique jamais avec une BMW. Il peut en revanche lire et écrire les
préférences du prototype ESP32-S3 par son port série USB. Le même fichier sert
au simulateur et à l'écriture du boîtier.

## Utilisation recommandée sous Windows

Depuis la racine du dépôt :

```powershell
.\scripts\configure.ps1
```

Le script compile l'outil puis ouvre l'assistant. La cible par défaut est
`config/user-settings.conf`, ignorée par Git. L'assistant propose les valeurs
actuelles entre crochets ; appuyer sur Entrée les conserve.

Le dernier écran propose d'éditer le catalogue modulaire. Ce passage est
facultatif ; s'il est choisi, chacune des 43 fonctionnalités possède sa propre
question oui/non et reste soumise aux barrières d'implémentation et de sûreté.

Une configuration typique pour le véhicule de référence sans capteur de capot
peut conserver trois appuis sur verrouillage, choisir `capot désactivé` et régler
librement la durée moteur entre 1 et 60 minutes. Les seuils de protection moteur
froid et de température de boîte sont proposés avec leurs bornes avant le
catalogue des fonctionnalités.

## Contrôler et simuler

```powershell
.\scripts\configure.ps1 -Check
.\scripts\simulate.ps1 -ConfigPath .\config\user-settings.conf
```

`-Check` affiche la forme canonique et termine par
`configuration_result: PASS`. Le simulateur doit terminer par
`scenario_result: PASS`.

## Commandes disponibles

```powershell
# Compiler seulement
.\scripts\build-configurator.ps1

# Afficher le fichier sans le modifier
.\scripts\configure.ps1 -Show

# Recréer explicitement les valeurs par défaut
.\scripts\configure.ps1 -WriteDefaults

# Utiliser un autre fichier
.\scripts\configure.ps1 -ConfigPath .\config\essai.local.conf

# Lister les ports COM présents sous Windows
.\scripts\configure.ps1 -ListDevices

# Lire et afficher les réglages du prototype
.\scripts\configure.ps1 -ReadDevice COM3

# Vérifier uniquement l'identité et les capacités du prototype
.\scripts\configure.ps1 -ProbeDevice COM3

# Écrire le fichier local, puis vérifier le résultat par relecture
.\scripts\configure.ps1 -WriteDevice COM3
```

L'exécutable accepte aussi directement `--show`, `--check`,
`--write-defaults`, `--print-defaults`, `--list-devices`,
`--probe-device PORT`, `--read-device PORT`, `--write-device PORT`,
`--config CHEMIN` et `--help`.

La détection liste volontairement tous les ports série actifs : elle ne devine
pas lequel correspond au boîtier. Cette sélection explicite évite d'écrire par
erreur sur un autre appareil. Le débit configuré est 115200 bauds ; l'USB CDC
natif de l'ESP32-S3 conserve néanmoins son propre transport USB.

## Garanties d'enregistrement

Avant toute écriture, toutes les bornes et les relations entre temporisations
sont validées. Le contenu est d'abord écrit dans un fichier temporaire, relu avec
le parseur strict puis comparé aux valeurs demandées. Une configuration existante
est préservée pendant le remplacement. En cas d'échec avant l'installation de la
nouvelle version, elle reste utilisable.

Les invariants non configurables restent ceux décrits dans
[user-configuration.md](user-configuration.md) et [safety.md](safety.md).
Le contrat de transfert est décrit dans
[settings-protocol.md](settings-protocol.md).

## Garanties de la liaison USB

- chaque échange possède un identifiant vérifié ;
- la signature produit, la cible, la version et les capacités sont sondées ;
- le temps de réponse total est limité à 2 secondes ;
- une trame partielle est abandonnée après 250 ms entre deux octets ;
- longueur, version et CRC sont contrôlés avant d'interpréter une réponse ;
- un statut `busy`, `unauthorized` ou une panne de stockage est affiché comme
  un échec, jamais comme un succès ;
- une écriture réussie est immédiatement suivie d'une lecture complète et
  d'une comparaison champ par champ ;
- aucune commande moteur ou véhicule n'existe dans ce protocole.
