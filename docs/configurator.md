# Configurateur utilisateur

## Rôle actuel

`bmw_remote_configurator.exe` est un outil PC hors véhicule. Il permet de créer
et modifier les préférences sans connaître la syntaxe du fichier `key=value`.
Il utilise le même `UserSettings` et le même validateur que le contrôleur.

Il ne communique actuellement ni avec une BMW ni avec un microcontrôleur. Le
fichier produit sert au simulateur. Le codec et le service de dialogue avec le
futur boîtier sont maintenant définis, mais l'adaptateur USB, Bluetooth ou réseau
reste à choisir et à implémenter.

## Utilisation recommandée sous Windows

Depuis la racine du dépôt :

```powershell
.\scripts\configure.ps1
```

Le script compile l'outil puis ouvre l'assistant. La cible par défaut est
`config/user-settings.conf`, ignorée par Git. L'assistant propose les valeurs
actuelles entre crochets ; appuyer sur Entrée les conserve.

Une configuration typique pour le véhicule de référence sans capteur de capot
peut conserver trois appuis sur verrouillage, choisir `capot désactivé` et régler
librement la durée moteur entre 1 et 60 minutes.

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
```

L'exécutable accepte aussi directement `--show`, `--check`,
`--write-defaults`, `--print-defaults`, `--config CHEMIN` et `--help`.

## Garanties d'enregistrement

Avant toute écriture, toutes les bornes et les relations entre temporisations
sont validées. Le contenu est d'abord écrit dans un fichier temporaire, relu avec
le parseur strict puis comparé aux valeurs demandées. Une configuration existante
est préservée pendant le remplacement. En cas d'échec avant l'installation de la
nouvelle version, elle reste utilisable.

Les invariants non configurables restent ceux décrits dans
[user-configuration.md](user-configuration.md) et [safety.md](safety.md).
Le contrat de transfert futur est décrit dans
[settings-protocol.md](settings-protocol.md).
