# Simulateur applicatif hors véhicule

## Objectif

Le simulateur exécute le vrai contrôleur, sa politique de sécurité, le runtime et
les adaptateurs de rejeu avec un protocole synthétique réservé au PC. Il ne se
connecte à aucun câble, bus, GPIO ou véhicule.

## Interface graphique Windows

L'interface locale constitue le chemin recommandé pour découvrir le projet :

```powershell
.\scripts\simulator-gui.ps1
```

On peut également ouvrir `scripts/start-simulator-gui.cmd` par double clic.
Le script reconstruit l'exécutable uniquement si une source est plus récente,
puis présente :

- les scénarios documentés avec leur description ;
- le résultat réussi ou échoué et la sortie technique complète ;
- la sélection d'un fichier de configuration utilisateur ;
- l'inspection d'une trace canonique avec contrôle du capot requis ou facultatif ;
- la copie du rapport dans le presse-papiers.

L'interface ne démarre aucun serveur, n'utilise pas le réseau et n'ajoute aucune
dépendance : elle s'appuie sur Windows PowerShell et WinForms déjà présents sur
Windows. Son contrat avec la CLI peut être vérifié sans ouvrir de fenêtre :

```powershell
.\scripts\simulator-gui.ps1 -SelfTest
```

Les mainteneurs peuvent également produire une prévisualisation déterministe
sans interaction avec `-PreviewPath .\build\simulator-gui-preview.png`.

## Console et exécutable

```powershell
.\scripts\build-simulator.ps1
```

L'exécutable est créé ici :

```text
build/bmw_remote_simulator.exe
```

Il peut être lancé directement. Sans argument, il affiche un menu interactif :

1. démarrage puis arrêt nominaux ;
2. capot obligatoire, ouverture conduisant au défaut attendu ;
3. capot facultatif, ouverture entièrement ignorée ;
4. portière ouverte sans reprise confirmée, puis arrêt à l'échéance ;
5. portière ouverte avec reprise conducteur authentifiée ;
6. perte des mises à jour carrosserie pendant le fonctionnement distant ;
7. retard des signaux au-delà de leur fraîcheur avant lancement ;
8. corruption d'une trame reconnue pendant l'autorisation ;
9. chargement et exécution d'un fichier de configuration utilisateur ;
10. corruption du réglage récent et récupération de la génération précédente ;
11. liaison de configuration avec autorisation, état occupé et corruption ;
12. refus des preuves de verrouillage non fiables, anciennes ou rejouées ;
13. décodage de fronts et compteur roulant sur un vecteur CAN fictif ;
14. séquence d'actionneurs simulés, expiration du heartbeat et retour incohérent ;
15. chaîne complète contrôleur, runtime et superviseur avec propagation du défaut.

## Lancer un scénario depuis PowerShell

```powershell
.\scripts\simulate.ps1 -Scenario nominal
.\scripts\simulate.ps1 -Scenario hood-required
.\scripts\simulate.ps1 -Scenario hood-optional
.\scripts\simulate.ps1 -Scenario takeover-timeout
.\scripts\simulate.ps1 -Scenario takeover-confirmed
.\scripts\simulate.ps1 -Scenario signal-loss
.\scripts\simulate.ps1 -Scenario signal-delay
.\scripts\simulate.ps1 -Scenario frame-corruption
.\scripts\simulate.ps1 `
  -ConfigPath .\config\user-settings.example.conf
.\scripts\simulate.ps1 -Scenario settings-recovery
.\scripts\simulate.ps1 -Scenario settings-link
.\scripts\simulate.ps1 -Scenario lock-replay-guard
.\scripts\simulate.ps1 -Scenario qualified-lock-adapter
.\scripts\simulate.ps1 -Scenario actuator-supervisor
.\scripts\simulate.ps1 -Scenario supervised-runtime
```

Chaque scénario retourne le code `0` uniquement si le résultat final correspond
au comportement attendu et affiche `scenario_result: PASS`.

Les scénarios qui utilisent le runtime affichent aussi `diagnostic_log`. Les
entrées sont ordonnées de la plus ancienne à la plus récente et montrent le temps
monotone, la commande ou transition, le défaut et le motif du refus. Le compteur
`overwritten` indique combien d'anciennes entrées auraient été remplacées. Voir
[diagnostic-journal.md](diagnostic-journal.md).

## Campagnes d'anomalies véhicule

Les trois injections sont déterministes et restent limitées au protocole
synthétique hors véhicule :

- `signal-loss` supprime la mise à jour carrosserie à 5 secondes. La mise à jour
  groupe motopropulseur reste fraîche, mais les signaux carrosserie dépassent
  leur âge maximal. Le contrôleur doit passer de `Running` à `Fault` avec
  `SafetyInterlock` et sécuriser les sorties ;
- `signal-delay` avance l'horloge jusqu'à la fin de préparation sans fournir les
  messages attendus. Le régime et la transmission deviennent périmés, le
  démarreur ne doit jamais être engagé et le contrôleur doit échouer fermé ;
- `frame-corruption` altère la signature d'une trame carrosserie reconnue lors de
  l'autorisation. Le décodeur doit la rejeter, le gateway doit échouer et le
  runtime doit convertir cet échec en `VehicleCommunication` puis `Fault`.

Chaque scénario vérifie également la présence de `SecureOutputs`. Ces campagnes
ne prétendent pas représenter les identifiants, périodicités ou protections des
trames BMW réelles.

Le parcours configuré charge toutes les valeurs, simule le nombre d'appuis choisi,
affiche la durée moteur réellement armée puis applique la stratégie d'ouverture
de portière. Voir [user-configuration.md](user-configuration.md).

Le scénario `settings-recovery` utilise le vrai journal binaire, corrompt le CRC
du second emplacement puis vérifie que le premier est restauré. Voir
[settings-persistence.md](settings-persistence.md).

Le scénario `settings-link` fait transiter une configuration complète octet par
octet dans le vrai récepteur et le vrai endpoint. Il vérifie successivement le
refus sans autorisation, le refus d'écriture pendant `Running`, l'enregistrement
pendant `Idle`, la lecture autorisée, l'émission de chaque réponse et le rejet
d'un CRC corrompu sans réponse. Voir
[settings-protocol.md](settings-protocol.md).

Le scénario `lock-replay-guard` vérifie que la source synthétique est refusée
par défaut, puis injecte une source candidate, une preuve périmée, un compteur
dupliqué et un compteur désordonné. Il termine par trois preuves synthétiques
explicitement autorisées et strictement croissantes. Voir
[lock-command-security.md](lock-command-security.md).

Le scénario `qualified-lock-adapter` utilise un identifiant fictif qui
n'appartient pas à BMW. Il vérifie le refus d'une liaison candidate, l'unicité
du front lorsque la commande reste active, le rebouclage du compteur,
l'annulation du geste après rejeu et l'acceptation de trois nouveaux fronts.
Voir [can-lock-command-adapter.md](can-lock-command-adapter.md).

Le scénario `actuator-supervisor` utilise un pilote sans GPIO. Il confirme une
séquence allumage/démarreur, suspend ensuite le heartbeat jusqu'à la mise en
sécurité, effectue un réarmement contrôlé puis simule un retour électrique qui
n'a pas suivi la commande. Voir
[actuator-safety-supervisor.md](actuator-safety-supervisor.md).

Le scénario `supervised-runtime` emploie ce même superviseur comme véritable
`ActuatorPort` du runtime. Il parcourt l'autorisation, l'allumage, le lancement
et la confirmation moteur, puis suspend le heartbeat. La mise en sécurité locale
est ensuite propagée sous forme d'`ActuatorFailure` jusqu'à l'état applicatif
`Fault` et au journal de diagnostic.

## Inspecter une trace

La syntaxe historique reste acceptée :

```powershell
.\scripts\simulate.ps1 .\scenarios\synthetic_idle.cantrace.csv
```

Le contrôle du capot peut être choisi pour l'inspection :

```powershell
.\scripts\simulate.ps1 `
  -TracePath .\scenarios\synthetic_idle.cantrace.csv `
  -Hood optional
```

La CLI directe équivalente est :

```powershell
.\build\bmw_remote_simulator.exe `
  --trace .\scenarios\synthetic_idle.cantrace.csv `
  --hood optional
```

Une trace BMW réelle peut être chargée, mais ses trames restent ignorées tant
qu'un décodeur BMW en lecture seule n'a pas été qualifié.

## Limites

- les actionneurs affichés sont des objets console ou simulés sans sortie physique ;
- l'interface graphique est actuellement destinée à Windows ;
- les identifiants synthétiques ne sont pas des identifiants BMW ;
- un succès du simulateur ne qualifie ni le matériel ni une installation ;
- l'exécutable produit localement n'est pas ajouté au dépôt Git.
