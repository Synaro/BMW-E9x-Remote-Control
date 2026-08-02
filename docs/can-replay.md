# Rejeu CAN hors véhicule

## Objectif

Le moteur de rejeu alimente le modèle véhicule avec une chronologie déterministe
de trames CAN, sans accéder à un bus physique. Il permet de reproduire une
séquence, injecter un défaut et vérifier les décisions du contrôleur sur un poste
de développement ou dans la CI.

Il s'agit exclusivement d'un mécanisme de **lecture et de simulation**. Aucune
classe de ce module ne sait transmettre une trame.

## Contrat d'une trace

Une trace est un tableau contigu de `CanFrame` :

- horodatage relatif en millisecondes ;
- identifiant standard 11 bits ou étendu 29 bits ;
- longueur de zéro à huit octets ;
- charge utile de taille fixe ;
- horodatages triés dans l'ordre croissant ou égal.

Le moteur bas niveau accepte une trace vide. L'importeur de fichiers exige au
moins une trame afin d'éviter qu'une capture vide soit prise pour une validation
réussie. Un pointeur nul avec une taille non nulle, un
identifiant hors plage, une longueur supérieure à huit ou un horodatage non
monotone rend toute la trace invalide avant le premier rejeu.

`advanceTo(t)` émet uniquement les trames dont l'horodatage est inférieur ou égal
à `t`. Une trame refusée reste au curseur afin de rendre l'échec reproductible.
L'horloge de rejeu doit elle aussi progresser de façon monotone ; un retour en
arrière est mémorisé comme une erreur jusqu'à `reset()`.

## Décodage atomique

Un `CanFrameDecoder` classe chaque trame :

- `Ignored` : identifiant inconnu, aucun effet ;
- `Decoded` : lot de signaux candidat ;
- `Invalid` : trame reconnue mais incorrecte, arrêt du rejeu.

Le `DecodedSignalBatch` contient au maximum dix signaux, soit la taille actuelle
du modèle véhicule. `VehicleStateAssembler` valide toutes les valeurs avant de
modifier son état. Cette validation en deux phases évite une mise à jour partielle
si le dernier champ d'une trame est corrompu.
Deux valeurs du même signal dans un lot sont rejetées comme ambiguës.

## Fraîcheur

Chaque famille de signaux possède une durée de validité configurable :

| Famille | Valeur par défaut |
|---|---:|
| Régime moteur | 500 ms |
| Transmission / rapport | 1 000 ms |
| Batterie | 2 000 ms |
| Carrosserie | 2 000 ms |
| Défaut critique | 2 000 ms |

Un signal jamais reçu est `Unavailable`. Après sa durée maximale il devient
`Stale`. Seul `Fresh` peut participer à une autorisation de sécurité.

## Protocole synthétique

Le décodeur de démonstration reconnaît deux identifiants étendus réservés au
projet :

| Identifiant | Contenu synthétique |
|---:|---|
| `0x1FFFFF00` | régime, batterie, transmission, rapport, défaut critique |
| `0x1FFFFF01` | capot, portes, coffre, frein, frein de stationnement |

Chaque trame porte la signature `0xA5` dans le dernier octet. Ces identifiants ne
sont pas des identifiants BMW et ne doivent jamais être transmis à un véhicule.

Les scénarios de `tools/vehicle_simulator.cpp` utilisent uniquement ce protocole.
Ils peuvent être exécutés avec `scripts/simulate.ps1` ou depuis l'exécutable
interactif décrit dans [simulator.md](simulator.md).

## Import de captures

Le format hôte canonique et le convertisseur `python-can` sont décrits dans
[trace-import.md](trace-import.md). Le parseur de fichiers reste lié aux outils
PC et n'est pas compilé dans le firmware. Le décodeur BMW futur restera dans
l'infrastructure, jamais dans le domaine.
