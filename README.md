# BMW E9x Remote Control

Contrôleur embarqué expérimental pour orchestrer des fonctions distantes sur BMW
Série E9x à partir d'un cœur logiciel déterministe, testable et indépendant du
matériel.

> [!WARNING]
> Ce dépôt n'est pas un produit prêt à installer dans un véhicule. Le firmware
> fourni reste interdit de connexion au véhicule : l'acquisition TWAI passive
> est désactivée par défaut et réservée à un bus de banc qualifié. Aucun GPIO
> d'actionneur réel n'est configuré. Toute intégration physique exige une analyse de risques, des
> interverrouillages matériels, des essais sur banc et l'intervention d'une
> personne qualifiée. Le projet ne doit pas servir à contourner un antidémarrage,
> un système antivol ou une réglementation applicable.

## État du projet

### Implémenté et vérifié hors véhicule

- cœur déterministe : modèle d'état, politique de sécurité, machine d'état,
  décisions bornées et ports d'infrastructure ;
- configuration utilisateur versionnée, validée, protégée par CRC et persistée
  dans NVS sur ESP32-S3 ;
- configurateur et simulateur Windows, scénarios synthétiques, rejeu de traces
  et injections de défauts ;
- détection abstraite de trois impulsions de verrouillage avec gardes de
  provenance, fraîcheur et ordre, mais sans liaison BMW qualifiée ;
- couche CAN générique, file RX fixe et statistiques ; le pilote ESP-IDF/TWAI
  reste désactivé par défaut, démarre en `TWAI_MODE_LISTEN_ONLY` par défaut et
  peut sélectionner explicitement `TWAI_MODE_NORMAL` sur le profil Phase 3H,
  sans exposer encore aucune API d'émission ;
- contrats Capture V2, imports tabulaires et base de preuves Phase 3C/3D en
  lecture seule ;
- télémétrie V1 limitée aux trois fonctions réellement implémentées, toutes
  désactivées par défaut ;
- suites C++, Python, PlatformIO native/ESP32-S3 et simulateur Windows exécutées
  en intégration continue.

Ces validations sont logicielles et hors véhicule. Elles ne qualifient ni le
montage électrique K-CAN, ni une fonction de démarrage réel.

### Phase 3F/3G.1 — révisions K-CAN RX-only historiques

- design `KCAN_RX_ONLY_P3F` gelé, approvisionnement audité et PCB Phase 3G.1
  conservé comme révision historique reproductible ;
- carte d'achat actuelle : **ESP32-S3-DevKitC-1-N8R8** ; les 8 MiB de PSRAM
  supplémentaires ne sont pas utilisés par le firmware ;
- composants non commandés, montage non assemblé et non qualifié
  électriquement ;
- `R_LINK_TX` physiquement DNP : aucune pièce, aucun cavalier et aucune
  continuité TX autorisés ;
- aucune connexion à la BMW avant un PASS électrique complet et documenté ;
- aucune Phase 4 active. Cette révision ne doit plus être utilisée pour une
  nouvelle fabrication.

Les sources normatives sont volontairement séparées :

- [`BOM.csv`](hardware/kcan-rxonly/BOM.csv) décrit ce qui est utilisé ;
- [`procurement.csv`](hardware/kcan-rxonly/procurement.csv) décrit ce qui doit
  être acheté, avec les SKU, quantités économiques et snapshots datés ;
- [`netlist.csv`](hardware/kcan-rxonly/netlist.csv) décrit les connexions ;
- [`wiring.md`](hardware/kcan-rxonly/wiring.md) décrit l'assemblage et les
  contrôles. En cas de divergence, aucun résumé du README ne remplace ces
  fichiers.

### Phase 3H — matériel K-CAN entièrement bidirectionnel

La carte destinée à une future fabrication est désormais la Phase 3H :

- `ESP32-S3 GPIO5/TWAI_TX -> SN74LXC1T45-Q1 -> TJA1055 TXD` est un chemin
  cuivre continu ;
- `TJA1055 RXD -> ESP32-S3 GPIO4/TWAI_RX` reste le chemin de réception ;
- aucun jumper, pont de soudure, composant DNP ou gap cuivre ne verrouille TX ;
- le DevKit reste alimenté par USB et son 5 V n'est pas relié à `5V_TJA` ;
- le rail `5V_TJA` est fourni par le `TLS715B0EJV50XUMA1` validé en Phase 3G.1 ;
- les Gerbers, drills, BOM/CPL JLCPCB, BOM/centroid NextPCB, STEP, PDF, rendus
  et manifeste SHA-256 sont régénérés sous
  [`hardware/kcan-bidirectional-pcb/`](hardware/kcan-bidirectional-pcb/) ;
- aucune commande fabricant n'a été passée et aucune émission BMW n'est
  autorisée par cette révision.

La justification électrique, les GPIO, les marges logiques et les limites sont
documentés dans
[`docs/phase3h-bidirectional-kcan.md`](docs/phase3h-bidirectional-kcan.md).

### Non implémenté ou interdit à ce stade

L'adaptateur BMW réel de verrouillage, les actionneurs, les commandes
CAS/DDE/KL50 et le remote-start actif restent absents. Le **chemin électrique**
TX existe désormais sur la carte Phase 3H, mais aucune API ou commande
d'émission véhicule n'est implémentée.
Les observations communautaires ou diagnostiques ne deviennent jamais une
commande. Aucun essai BMW, rejeu ou contournement CAS/EWS/antidémarrage n'est
autorisé par l'état actuel du dépôt.

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
La frontière de confiance et les limites anti-rejeu des commandes sont décrites
dans [docs/lock-command-security.md](docs/lock-command-security.md).
Le contrat du futur décodeur CAN de verrouillage est défini dans
[docs/can-lock-command-adapter.md](docs/can-lock-command-adapter.md).
Les préférences, leurs limites et leur future persistance sont décrites dans
[docs/user-configuration.md](docs/user-configuration.md).
Le catalogue des fonctions actuellement implémentées est décrit dans
[docs/feature-framework.md](docs/feature-framework.md). Les idées non livrées
sont isolées dans [docs/backlog/features.md](docs/backlog/features.md).
Le premier moteur de télémétrie et ses alertes sont décrits dans
[docs/telemetry-alerts.md](docs/telemetry-alerts.md).
Le journal binaire redondant est spécifié dans
[docs/settings-persistence.md](docs/settings-persistence.md).
Le socle ESP-IDF, ses garde-fous et sa procédure de banc sont documentés dans
[docs/esp32s3-safe-foundation.md](docs/esp32s3-safe-foundation.md).
L'acquisition passive de Phase 3 et le format Capture V2 sont décrits dans
[docs/twai-listen-only-acquisition.md](docs/twai-listen-only-acquisition.md) et
[docs/capture-format-v2.md](docs/capture-format-v2.md).
Le choix du transceiver, le schéma de banc et la qualification électrique de
Phase 3B sont figés dans
[docs/phase3b-can-bench-hardware.md](docs/phase3b-can-bench-hardware.md).
Les contrats d'intégration des futurs relevés ISTA/TestO, la provenance et la
checklist de Phase 3C sont décrits dans
[docs/phase3c-vehicle-data-intake.md](docs/phase3c-vehicle-data-intake.md).
La qualification des premières preuves réelles et les inconnues restantes sont
décrites dans
[docs/phase3d-real-evidence-qualification.md](docs/phase3d-real-evidence-qualification.md).
La préparation K-CAN passive et le design RX-only gelé sont documentés dans
[docs/phase3e-kcan-passive-acquisition.md](docs/phase3e-kcan-passive-acquisition.md)
et
[docs/phase3f-kcan-rxonly-design-freeze.md](docs/phase3f-kcan-rxonly-design-freeze.md).
L'arrêt de conception PCB Phase 3G est motivé dans
[docs/phase3g-pcb-feasibility-audit.md](docs/phase3g-pcb-feasibility-audit.md).
La révision bidirectionnelle qui remplace ces cartes pour toute future
fabrication est décrite dans
[docs/phase3h-bidirectional-kcan.md](docs/phase3h-bidirectional-kcan.md).
Le configurateur PC est décrit dans
[docs/configurator.md](docs/configurator.md).
Le protocole entre configurateur et boîtier est spécifié dans
[docs/settings-protocol.md](docs/settings-protocol.md).
Le journal sémantique borné est décrit dans
[docs/diagnostic-journal.md](docs/diagnostic-journal.md).
La cible matérielle de banc, les achats autorisés et les limites avant connexion
au véhicule sont détaillés dans
[docs/hardware-v1-reference.md](docs/hardware-v1-reference.md).
La barrière logicielle placée devant le futur pilote de sorties est spécifiée
dans [docs/actuator-safety-supervisor.md](docs/actuator-safety-supervisor.md).
Le protocole local du bac à sable graphique est documenté dans
[docs/sandbox-protocol.md](docs/sandbox-protocol.md).

## Validation locale

Prérequis : un compilateur C++17. Sous Windows avec `g++.exe` disponible :

```powershell
./scripts/test.ps1
```

Les tests de l'importeur Python n'installent aucune dépendance externe :

```powershell
python -m unittest discover -s tools -p 'test_*.py' -v
```

Compilation du firmware natif avec PlatformIO :

```powershell
pio run -e native
```

La cible exacte du prototype ESP32-S3 peut être vérifiée avec :

```powershell
pio run -e esp32s3dev
```

Cette carte est la cible du prototype de banc, pas celle de l'installation
automobile définitive. Le firmware dessert le protocole local de configuration
sur USB. L'acquisition TWAI n'est activable que par une configuration locale
`BENCH_ONLY` et ne possède aucun chemin d'émission ; aucune sortie véhicule
n'est activée.

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
libs/can-core/                       Modèle et réception CAN génériques, sans BMW
src/                                Implémentations et firmware ESP32-S3
tests/                              Scénarios hôte
tools/                              Simulateur et importeur de traces PC
vehicle-data/                       Profils, observations, preuves et imports versionnés
scenarios/                          Traces synthétiques partageables
docs/                               Architecture, sécurité et intégration
```

## Configurateur utilisateur Windows

Le fichier personnel peut être créé ou modifié sans éditer de texte à la main :

```powershell
.\scripts\configure.ps1
```

L'assistant conserve une valeur lorsque l'utilisateur appuie simplement sur
Entrée, valide toutes les limites de sûreté puis écrit
`config/user-settings.conf`. Le contrôle du capot peut notamment être placé sur
`disabled` pour une E9x qui ne possède pas ce capteur.

La configuration produite peut être contrôlée puis exécutée immédiatement dans
le simulateur :

```powershell
.\scripts\configure.ps1 -Check
.\scripts\simulate.ps1 -ConfigPath .\config\user-settings.conf
```

L'exécutable généré se trouve dans `build/bmw_remote_configurator.exe`. Le codec
et le service communiquent avec le prototype par son port série USB. Pour lister
les ports puis lire ou écrire le boîtier :

```powershell
.\scripts\configure.ps1 -ListDevices
.\scripts\configure.ps1 -ProbeDevice COM3
.\scripts\configure.ps1 -ReadDevice COM3
.\scripts\configure.ps1 -WriteDevice COM3
```

Une écriture n'est déclarée réussie qu'après relecture et comparaison complète.
Voir [docs/configurator.md](docs/configurator.md).

Le premier essai avec une carte neuve est automatisé et utilise une
configuration de banc où le démarrage distant reste désactivé :

```powershell
.\scripts\first-usb-test.ps1 -Port COM3
```

## Simulateur applicatif

Sous Windows, le moyen le plus simple est l'interface graphique :

```powershell
.\scripts\simulator-gui.ps1
```

Le fichier `scripts/start-simulator-gui.cmd` peut aussi être ouvert par double
clic. L'interface compile le simulateur uniquement lorsque ses sources sont
plus récentes, permet de choisir un scénario, un fichier de configuration ou
une trace, puis affiche et copie le rapport. Le bouton **Ouvrir le bac à sable**
garde une session vivante : l'utilisateur peut modifier les portes, le capot,
le frein, le rapport et le régime moteur, déclencher les timers et observer en
direct l'état, les sorties simulées et les défauts. Les températures moteur et
boîte, la régénération FAP et leurs trois fonctions V1 peuvent également être
injectées ou désactivées séparément. Elle reste entièrement
locale et ne se connecte à aucun véhicule.

Sous Windows, l'exécutable interactif peut être construit avec :

```powershell
.\scripts\build-simulator.ps1
.\build\bmw_remote_simulator.exe
```

Le catalogue des fonctions actuellement implémentées est affichable
sans lancer de scénario :

```powershell
.\build\bmw_remote_simulator.exe --list-features
```

Quinze scénarios permettent de tester le parcours nominal, le capot obligatoire
ou facultatif, la reprise conducteur, la perte, le retard et la corruption des
données, un profil utilisateur, la récupération du stockage et la liaison de
configuration, la garde anti-rejeu et l'adaptateur CAN qualifié sur un vecteur
fictif, le watchdog et les retours d'actionneurs simulés, ainsi que leur
propagation complète dans le runtime. Un scénario précis
peut aussi être compilé et exécuté avec :

```powershell
.\scripts\simulate.ps1 -Scenario hood-optional
.\scripts\simulate.ps1 -Scenario takeover-timeout
.\scripts\simulate.ps1 -Scenario takeover-confirmed
.\scripts\simulate.ps1 -Scenario signal-loss
.\scripts\simulate.ps1 -Scenario signal-delay
.\scripts\simulate.ps1 -Scenario frame-corruption
.\scripts\simulate.ps1 -ConfigPath .\config\user-settings.example.conf
.\scripts\simulate.ps1 -Scenario settings-recovery
.\scripts\simulate.ps1 -Scenario settings-link
.\scripts\simulate.ps1 -Scenario lock-replay-guard
.\scripts\simulate.ps1 -Scenario qualified-lock-adapter
.\scripts\simulate.ps1 -Scenario actuator-supervisor
.\scripts\simulate.ps1 -Scenario supervised-runtime
```

Chaque parcours affiche aussi le journal chronologique des commandes,
transitions, refus et défauts. Le résultat attendu est affiché sous la forme
`scenario_result: PASS` et le code de sortie reste non nul si le comportement
diverge.

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
