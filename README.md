# BMW E9x Remote Control

Contrôleur embarqué expérimental pour orchestrer des fonctions distantes sur BMW
Série E9x à partir d'un cœur logiciel déterministe, testable et indépendant du
matériel.

> [!WARNING]
> Ce dépôt n'est pas un produit prêt à installer dans un véhicule. Le firmware
> fourni est volontairement inerte : aucun bus véhicule, GPIO ou actionneur réel
> n'est configuré. Toute intégration physique exige une analyse de risques, des
> interverrouillages matériels, des essais sur banc et l'intervention d'une
> personne qualifiée. Le projet ne doit pas servir à contourner un antidémarrage,
> un système antivol ou une réglementation applicable.

## État du projet

Le premier jalon logiciel est opérationnel :

- modèle d'état du véhicule avec qualité explicite des signaux ;
- politique de sécurité indépendante du matériel ;
- machine d'état événementielle complète ;
- détection applicative configurable de trois impulsions de verrouillage ;
- profil utilisateur validé avant application, sans recompilation du noyau ;
- décisions et listes d'actions de taille fixe, sans allocation dynamique ;
- arrêt fail-safe en cas de défaut d'un adaptateur ;
- ports abstraits pour véhicule, actionneurs, minuterie et notifications ;
- firmware ESP32 de référence inerte ;
- rejeu temporel de traces CAN et assemblage des signaux avec gestion de fraîcheur ;
- protocole CAN synthétique réservé aux simulations hors véhicule ;
- simulateur interactif avec parcours nominal et injection d'un défaut de sécurité ;
- session distante limitée à 15 minutes et reprise conducteur bornée à 60 secondes ;
- profils véhicule extensibles avec qualification fermée par défaut ;
- profil de découverte pour l'E90 2009 N47D20C boîte automatique ;
- absence de capteur de capot déclarée pour ce véhicule de référence, contrôle activable au choix de l'utilisateur ;
- import PC de journaux `python-can` vers un format canonique strict ;
- capture PC bornée avec refus des interfaces sans mode silencieux documenté ;
- sélection explicite d'un profil obligatoire dans le contrôleur ;
- analyse différentielle hors ligne des octets et bits candidats ;
- 54 tests C++, 6 parcours CLI et 21 tests Python automatisés en intégration continue.

L'adaptateur BMW qui observera réellement le verrouillage, qualifiera la reprise
conducteur et pilotera les sorties physiques reste à implémenter lorsque le
matériel, les signaux et les critères d'acceptation auront été précisément
définis.

Le moteur de rejeu accepte des tableaux bornés et des fichiers de trace
canoniques. Aucun identifiant BMW n'est supposé : le profil de référence reste
au niveau `Discovery` et toutes ses sources de signaux restent `Candidate`.

## Architecture

```text
Commande / véhicule / timers
             |
             v
        Runtime embarqué  ------>  Ports d'infrastructure
             |                    (bus, E/S, notifications)
             v
        Controller
             |
             +------> SafetyPolicy
             |
             v
      Decision + actions bornées
```

Les dépendances vont vers les abstractions : la couche application connaît le
modèle du véhicule, mais ne connaît ni Arduino, ni ESP32, ni CAN, ni GPIO.

La machine d'état utilise les états suivants : `Idle`, `Authorizing`,
`Preparing`, `Cranking`, `Running`, `AwaitingTakeover`, `DriverControl`,
`Stopping` et `Fault`. Une donnée nécessaire absente ou périmée provoque un
refus ou un passage en défaut selon le contexte.

La description détaillée se trouve dans [docs/architecture.md](docs/architecture.md)
et les invariants dans [docs/safety.md](docs/safety.md). Le système de variantes
est décrit dans [docs/vehicle-profiles.md](docs/vehicle-profiles.md).
Le déclenchement par trois verrouillages et le cycle de reprise sont spécifiés
dans [docs/remote-session.md](docs/remote-session.md).
Les préférences, leurs limites et leur future persistance sont décrites dans
[docs/user-configuration.md](docs/user-configuration.md).

## Validation locale

Prérequis : un compilateur C++17. Sous Windows avec `g++.exe` disponible :

```powershell
./scripts/test.ps1
```

Les tests de l'importeur Python n'installent aucune dépendance externe :

```powershell
python -m unittest discover -s tools -p 'test_*.py' -v
```

Compilation du firmware natif inerte avec PlatformIO :

```powershell
pio run -e native
```

La cible ESP32 de référence peut être vérifiée avec :

```powershell
pio run -e esp32dev
```

Cette cible ne constitue pas une sélection matérielle définitive et n'active
aucune sortie.

Avec CMake :

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Organisation

```text
include/bmw_remote/domain/          Modèle métier du véhicule
include/bmw_remote/application/     Sécurité, événements, décisions, contrôleur
include/bmw_remote/infrastructure/  Contrats des adaptateurs et runtime
include/bmw_remote/simulation/      Protocole synthétique hors véhicule
src/                                Implémentations et firmware inerte
tests/                              Scénarios hôte
tools/                              Simulateur et importeur de traces PC
scenarios/                          Traces synthétiques partageables
docs/                               Architecture, sécurité et intégration
```

## Simulateur applicatif

Sous Windows, l'exécutable interactif peut être construit avec :

```powershell
.\scripts\build-simulator.ps1
.\build\bmw_remote_simulator.exe
```

Six scénarios permettent de tester le parcours nominal, le capot obligatoire ou
facultatif, la reprise conducteur et un profil utilisateur complet. Un scénario précis
peut aussi être compilé et exécuté avec :

```powershell
.\scripts\simulate.ps1 -Scenario hood-optional
.\scripts\simulate.ps1 -Scenario takeover-timeout
.\scripts\simulate.ps1 -Scenario takeover-confirmed
.\scripts\simulate.ps1 -ConfigPath .\config\user-settings.example.conf
```

Le résultat attendu est affiché sous la forme `scenario_result: PASS` et le code
de sortie reste non nul si le comportement diverge.

Voir [docs/simulator.md](docs/simulator.md) pour la CLI et
[docs/can-replay.md](docs/can-replay.md) pour le contrat du moteur de rejeu.

Le simulateur accepte aussi une trace canonique externe :

```powershell
./scripts/simulate.ps1 ./scenarios/synthetic_idle.cantrace.csv
```

Pour convertir une capture reconnue par `python-can`, voir
[docs/trace-import.md](docs/trace-import.md). Les captures privées restent hors
du dépôt via `captures/private/`.

Une comparaison synthétique capot fermé / ouvert peut être lancée avec :

```powershell
./scripts/analyze-trace-change.ps1 `
  ./scenarios/synthetic_idle.cantrace.csv `
  ./scenarios/synthetic_hood_open.cantrace.csv
```

Le protocole destiné aux premières observations réelles est décrit dans
[docs/read-only-discovery.md](docs/read-only-discovery.md).

Une session privée d'inventaire K+DCAN peut être préparée sans enregistrer le
VIN :

```powershell
.\scripts\new-diagnostic-session.ps1 -SessionName e90-reference-01
```

Voir [docs/diagnostic-inventory.md](docs/diagnostic-inventory.md) avant d'ouvrir
une fonction dans ISTA, INPA ou Tool32.

Pour VS Code, PlatformIO génère localement `.vscode/c_cpp_properties.json` avec
les paramètres de l'environnement actif ; ce fichier reste hors Git car il
contient des chemins propres à la machine. `compile_flags.txt` configure le mode
de repli de `clangd`. Si d'anciens diagnostics restent affichés après une mise à
jour, exécuter `PlatformIO: Rebuild IntelliSense Index`, puis
`C/C++: Reset IntelliSense Database` et recharger la fenêtre.

## Capture CAN passive

Un outil PC peut produire directement une trace canonique depuis une interface
`python-can` dont le mode silencieux a été intégré explicitement :

```powershell
.\scripts\capture-can-trace.ps1 `
  -Interface pcan `
  -Channel PCAN_USBBUS1 `
  -Bitrate 500000 `
  -Duration 10 `
  -OutputPath .\captures\private\session-01\baseline.cantrace.csv
```

Le débit ci-dessus est uniquement un exemple et doit être confirmé avant la
connexion. L'outil ne contient aucun appel d'émission et refuse les pilotes non
qualifiés. Le câble K+DCAN reste réservé au diagnostic : il n'est pas supposé
fournir une capture CAN brute. Voir
[docs/capture-qualification.md](docs/capture-qualification.md).

## Principes de contribution

Toute évolution doit conserver la séparation entre métier et matériel, échouer
de manière sûre, rester déterministe et ajouter des tests aux nouvelles
transitions. Voir [CONTRIBUTING.md](CONTRIBUTING.md).

## Origine et inspiration

Le concept général est inspiré du dépôt
[AlbertoMarziali/bmw_remote_start](https://github.com/AlbertoMarziali/bmw_remote_start).
Ce projet est une implémentation indépendante : aucun code du dépôt de référence
n'a été copié. L'objectif est une architecture plus modulaire, explicite et
adaptée à une validation progressive.

## Licence

Distribué sous licence MIT. Voir [LICENSE](LICENSE).
