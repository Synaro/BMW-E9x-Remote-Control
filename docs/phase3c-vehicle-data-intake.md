# Phase 3C — Préparation des données véhicule

## Périmètre

La Phase 3C fournit des contrats de données et des outils hôte pour archiver des
identifications de calculateurs et des observations de démarrage OEM. Elle ne
contient aucune acquisition véhicule, aucun décodeur BMW, aucun identifiant CAN
BMW, aucune commande CAS/DDE et aucun chemin de transmission CAN.

Cette phase ne valide pas le remote-start. Elle prépare les preuves qui seront
nécessaires à de futures décisions, dans des phases explicitement autorisées.

## Modèle de données

```text
EvidenceIndex
    ^ evidence_ref
    |
    +--- VehicleProfile
    +--- OemStartObservationSession
    +--- RemoteStartPrerequisites

Generic delimited export + Mapping
    |
    +--- import_vehicle_data.py
             |
             +--- OemStartObservationSession
```

Le registre de preuves est le point d'ancrage. Une information observée ne peut
être élevée au-dessus de `UNKNOWN` que si elle possède une date et pointe vers
une preuve connue. Les valeurs restent atomiques : deux références provenant de
sources différentes peuvent donc avoir des niveaux de confiance différents
dans un même profil.

### Profil véhicule

Le profil conserve : VIN éventuellement anonymisé, modèle, châssis, date de
production, moteur, transmission, identification CAS, identification DDE et
calculateurs supplémentaires utiles. Pour CAS et DDE, le type/version, la
référence matérielle, la référence logicielle et le numéro ZB disposent chacun
de leur propre provenance.

La valeur `UNKNOWN` est représentée par `null`, sans date ni preuve. Une chaîne
vide ou une valeur inventée ne remplace jamais une donnée absente.

### Observation d'un démarrage OEM

Une session est une liste ordonnée de relevés. `sequence` lève l'ambiguïté quand
plusieurs signaux partagent le même `timestamp_us`. Les signaux autorisés sont
limités à ceux demandés pour les observations : KL15, KL50, régime, état moteur,
P/N, frein, état clé, autorisation OEM de démarrage et tension batterie.

Chaque relevé conserve la phase, la valeur brute, la valeur interprétée, l'unité,
la position exacte dans la preuve et le niveau de confiance. Le format ne
contient volontairement aucun champ d'identifiant CAN. Une future capture CAN
pourra être citée comme preuve uniquement après obtention réelle ; elle ne sera
pas décodée par cette phase.

`timestamp_basis` distingue un temps relatif de session, un timestamp absolu
fourni par la source et un temps indisponible. Dans ce dernier cas,
`timestamp_us` reste obligatoirement `null` : une transcription manuelle ne
reçoit jamais un horodatage fictif.

### Checklist de prérequis

Les statuts sont strictement :

- `UNKNOWN` : aucune preuve exploitable ;
- `OBSERVED` : vu au moins une fois et lié à une preuve ;
- `CONFIRMED` : observation revue et reproductible selon le protocole défini ;
- `VALIDATED` : critère d'acceptation futur satisfait et documenté ;
- `BLOCKED` : progression impossible, avec raison obligatoire.

Le validateur impose une preuve pour `OBSERVED`, `CONFIRMED` et `VALIDATED`.
Le changement de statut reste une décision humaine ; aucun import ne promeut
automatiquement un prérequis.

## Import générique

`tools/import_vehicle_data.py` lit uniquement des fichiers délimités décrits par
un mapping JSON. Le mapping associe des noms de colonnes quelconques aux champs
canoniques. Il ne contient aucune intégration propriétaire et ne connaît ni les
formats internes ni les API d'ISTA, TestO, INPA ou Tool32.

Les exports texte libres et notes manuelles restent des preuves brutes. Pour les
importer comme chronologie, l'utilisateur prépare une table intermédiaire sans
modifier l'original. La provenance pointe alors vers l'original et précise la
ligne ou le repère utilisé.

## Processus pour les futurs relevés réels

1. Copier le fichier brut dans `vehicle-data/evidence/private/` ou
   `vehicle-data/imports/private/`.
2. Conserver l'original intact et créer, si nécessaire, une copie anonymisée.
3. Ajouter la preuve à un index, avec outil, version, date, confidentialité et
   empreinte SHA-256.
4. Renseigner le profil ou préparer un mapping tabulaire.
5. Importer vers un fichier de session dans un emplacement de travail.
6. Valider ensemble l'index, le profil, la session et la checklist.
7. Revoir humainement les interprétations avant toute évolution de confiance.

## Barrières de phase

- une donnée sans preuve est refusée sauf si elle reste `UNKNOWN` ;
- une référence de preuve inconnue ou un chemin sortant du dossier est refusé ;
- les timestamps non monotones et séquences discontinues sont refusés ;
- les unités et types élémentaires incohérents sont refusés ;
- les données privées sont ignorées par Git par défaut ;
- aucun contenu de Phase 4 n'est implémenté ici ;
- aucune observation ne constitue une autorisation d'émettre sur le véhicule.
