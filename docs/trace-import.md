# Import de traces CAN

## Chaîne hors véhicule

L'importeur est un outil PC. Il convertit un journal reconnu par `python-can`
vers un CSV canonique strict, puis le simulateur rejoue ce fichier sans accéder
au véhicule :

```text
journal de capture -> import_can_trace.py -> .cantrace.csv -> simulate.ps1
```

Installer l'unique dépendance optionnelle :

```powershell
python -m pip install -r requirements-tools.txt
```

Convertir, par exemple, une capture ASC :

```powershell
python .\tools\import_can_trace.py `
  .\captures\private\session.asc `
  .\captures\private\session.cantrace.csv
```

`python-can` choisit le lecteur selon l'extension. La liste exacte dépend de sa
version ; sa documentation de référence décrit notamment les lecteurs ASC, BLF,
CSV, canutils LOG, SQLite, MF4 et TRC :
<https://python-can.readthedocs.io/en/stable/file_io.html>.

Analyser ensuite le fichier :

```powershell
.\scripts\simulate.ps1 .\captures\private\session.cantrace.csv
```

Le décodeur actuel reconnaît seulement le protocole synthétique documenté. Une
capture BMW valide sera donc importée et rejouée, mais ses trames seront comptées
comme ignorées tant que le décodeur BMW lecture seule n'aura pas été qualifié.
Ce comportement est volontaire et fermé par défaut.

Un exemple sans matériel est fourni :

```powershell
.\scripts\simulate.ps1 .\scenarios\synthetic_idle.cantrace.csv
```

## Format canonique

L'en-tête est fixe :

```csv
timestamp_ms,identifier,extended,dlc,data_hex
```

Règles appliquées par l'importeur Python et le lecteur C++ :

- trames de données CAN classique seulement, de zéro à huit octets ;
- identifiant 11 bits ou 29 bits cohérent avec `extended` ;
- premier horodatage relatif égal à zéro ;
- chronologie monotone dans un entier 32 bits ;
- DLC exactement égal à la charge utile ;
- limite dure du nombre de trames, un million par défaut ;
- rejet complet et atomique au premier champ invalide.

## Utilisation du câble K+DCAN

Le câble K+DCAN et les outils BMW sont pertinents pour inventorier en lecture
seule les calculateurs, valeurs de diagnostic et variantes. Ils ne produisent
pas nécessairement une capture passive brute directement compatible avec cet
importeur. EdiabasLib documente notamment l'accès aux fichiers BMW PRG/GRP et
aux adaptateurs FTDI K+DCAN : <https://github.com/uholeschak/ediabaslib>.

Pour ce jalon :

- ne lancer aucune fonction de codage, programmation ou actionneur ;
- ne pas utiliser le projet pour transmettre sur le bus ;
- travailler contact et alimentation selon une procédure maîtrisée ;
- conserver les captures dans `captures/private/`, ignoré par Git ;
- retirer VIN et données personnelles avant tout partage.

La prochaine étape de découverte consistera à définir une fiche d'observation
reproductible avec des événements sans danger, puis à corréler les changements
hors ligne avant d'écrire le moindre décodeur BMW.
