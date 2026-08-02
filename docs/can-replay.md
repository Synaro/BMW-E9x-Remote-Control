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

Une trace vide est valide. Un pointeur nul avec une taille non nulle, un
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

Le scénario de `tools/vehicle_simulator.cpp` utilise uniquement ce protocole. Il
peut être exécuté avec `scripts/simulate.ps1`.

## Ajout futur d'une capture réelle

Le format d'import ne sera choisi qu'après sélection de l'interface de capture
(par exemple un export texte de l'outil utilisé). L'importeur devra :

1. convertir les horodatages en temps relatif monotone ;
2. rejeter les lignes ambiguës ou tronquées ;
3. borner le nombre de trames et la taille du fichier ;
4. anonymiser les captures avant leur partage ;
5. rester dans les outils hôte, sans ajouter de parseur de fichiers au firmware ;
6. conserver le décodeur BMW dans l'infrastructure, jamais dans le domaine.
