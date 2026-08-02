# Simulateur applicatif hors véhicule

## Objectif

Le simulateur exécute le vrai contrôleur, sa politique de sécurité, le runtime et
les adaptateurs de rejeu avec un protocole synthétique réservé au PC. Il ne se
connecte à aucun câble, bus, GPIO ou véhicule.

## Construire l'exécutable Windows

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
6. chargement et exécution d'un fichier de configuration utilisateur ;
7. corruption du réglage récent et récupération de la génération précédente ;
8. liaison de configuration avec autorisation, état occupé et corruption.

## Lancer un scénario depuis PowerShell

```powershell
.\scripts\simulate.ps1 -Scenario nominal
.\scripts\simulate.ps1 -Scenario hood-required
.\scripts\simulate.ps1 -Scenario hood-optional
.\scripts\simulate.ps1 -Scenario takeover-timeout
.\scripts\simulate.ps1 -Scenario takeover-confirmed
.\scripts\simulate.ps1 `
  -ConfigPath .\config\user-settings.example.conf
.\scripts\simulate.ps1 -Scenario settings-recovery
.\scripts\simulate.ps1 -Scenario settings-link
```

Chaque scénario retourne le code `0` uniquement si le résultat final correspond
au comportement attendu et affiche `scenario_result: PASS`.

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

- les actionneurs affichés sont des objets console sans sortie physique ;
- les identifiants synthétiques ne sont pas des identifiants BMW ;
- un succès du simulateur ne qualifie ni le matériel ni une installation ;
- l'exécutable produit localement n'est pas ajouté au dépôt Git.
