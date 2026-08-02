# Qualification d'une capture CAN passive

## Portée

L'outil PC `capture_can_trace.py` reçoit des trames CAN classiques et les écrit
directement au format canonique du projet. Il ne contient aucun appel d'émission.
Il refuse aussi les pilotes pour lesquels aucun mode d'écoute seule documenté
n'a été intégré.

Cette garantie concerne le chemin logiciel du projet. Une interface matérielle
ne devient qualifiée pour le véhicule qu'après vérification de son câblage, de
son isolation, de sa terminaison et de son comportement réel sur banc.

## Interfaces actuellement acceptées

| Interface `python-can` | Mode exigé | Vérification |
|---|---|---|
| `pcan` | `BusState.PASSIVE` | l'état renvoyé par le pilote doit rester `PASSIVE` |
| `slcan` | `listen_only=True` | le pilote ouvre le canal avec la commande d'écoute seule |

Toute autre valeur est refusée. Ajouter un pilote exige une documentation
primaire démontrant son mode silencieux ainsi que des tests dédiés.

Le câble BMW K+DCAN n'est pas l'une de ces interfaces de capture brute. Il reste
réservé à l'inventaire diagnostic tant qu'une capacité différente n'a pas été
démontrée et revue.

## Installation

```powershell
python -m pip install -r requirements-tools.txt
```

## Exemple PCAN

L'exemple suivant illustre l'API ; le débit doit être confirmé pour le bus et le
point de raccordement réellement utilisés avant toute connexion :

```powershell
.\scripts\capture-can-trace.ps1 `
  -Interface pcan `
  -Channel PCAN_USBBUS1 `
  -Bitrate 500000 `
  -Duration 10 `
  -OutputPath .\captures\private\session-01\baseline.cantrace.csv
```

## Exemple SLCAN

```powershell
.\scripts\capture-can-trace.ps1 `
  -Interface slcan `
  -Channel COM8 `
  -Bitrate 500000 `
  -Duration 10 `
  -OutputPath .\captures\private\session-01\baseline.cantrace.csv
```

L'outil refuse d'écraser une trace existante sauf si `-Overwrite` est fourni. Il
échoue aussi si aucune trame n'est reçue, si la limite de trames est atteinte
avant la fin, si les horodatages reculent ou si une trame CAN FD, distante ou
d'erreur apparaît.

## Critères de qualification matérielle

Avant de cocher la qualification de l'interface dans la feuille de route :

1. identifier précisément l'interface, son firmware et son pilote ;
2. confirmer le mode silencieux dans la documentation du fabricant ;
3. vérifier sur banc qu'aucune trame et aucun acquittement ne sont émis ;
4. confirmer le débit, le raccordement, l'isolation et l'absence de terminaison
   supplémentaire non voulue ;
5. réaliser plusieurs captures de durée identique sans erreur ni perte visible ;
6. rejouer les fichiers avec le simulateur et contrôler leur format ;
7. consigner le résultat sans VIN ni donnée personnelle.

La présence de trames dans un fichier ne suffit pas, à elle seule, à qualifier
l'interface ni à confirmer la signification d'un identifiant.
