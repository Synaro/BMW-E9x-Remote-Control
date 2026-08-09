# Données véhicule et observations OEM — Phase 3C

Ce répertoire prépare l'intégration de relevés réels sans interpréter ni
commander le véhicule. Il ne contient ni identifiant CAN BMW supposé, ni
décodeur BMW, ni commande CAS/DDE, ni chemin d'émission CAN.

## Organisation

- `schema/` : contrats JSON versionnés ;
- `profiles/` : identité technique observée d'un véhicule et de ses calculateurs ;
- `observations/` : chronologies de démarrages OEM observés en lecture seule ;
- `evidence/` : index de provenance et preuves partageables anonymisées ;
- `imports/` : formats intermédiaires et mappings génériques pour données tabulaires.

Tous les fichiers suivis dont le nom commence par `EXAMPLE_ONLY` sont fictifs.
Ils démontrent uniquement le format. Ils ne décrivent aucun véhicule et ne
doivent jamais servir à déduire une valeur BMW.

Les fichiers `current-test-vehicle.*` contiennent uniquement des observations
réelles anonymisées et leur niveau de maturité actuel. La première observation
Tool32 confirme la correspondance de `KLEMMENSTATUS` avec KL15. Elle consigne
aussi explicitement que la valeur 69 existe moteur arrêté comme moteur tournant
et ne constitue donc pas un signal d'état moteur.

Les fichiers Phase 3D ajoutent les identifications CAS/DDE/EGS, les observations
Tool32/ISTA, les séquences RPM TestO et une timeline RPM/MSA synchronisée. Les
valeurs sont qualifiées `OBSERVED`, `CONFIRMED`, `UNTRUSTED` ou `BLOCKED`. Une
donnée `UNTRUSTED` ou `BLOCKED` ne peut jamais être marquée candidate par le
validateur.

La qualification de source précise également l'usage permis ou interdit. Une
source confirmée pour KL15 peut donc rester impropre à l'état moteur. Les
bitfields synchronisés conservent les formes décimale, hexadécimale et binaire,
ainsi que les bits modifiés. Une corrélation temporelle reste une corrélation :
elle ne peut pas devenir automatiquement une signification fonctionnelle.

## Règles pour les données réelles

1. Placer d'abord les exports bruts et captures dans un sous-dossier `private/`.
   Ces dossiers sont ignorés par Git afin de réduire le risque de publier un VIN
   ou une autre donnée personnelle.
2. Créer une entrée dans l'index de preuves. Calculer son SHA-256 si le fichier
   doit être figé comme preuve.
3. Anonymiser la copie destinée au dépôt. Un VIN complet n'est jamais requis.
4. Faire pointer chaque valeur retenue vers une ou plusieurs preuves, avec date
   d'observation et confiance explicite.
5. Lancer le validateur avant tout commit.

```powershell
python tools/vehicle_data_validation.py `
  --evidence vehicle-data/evidence/EXAMPLE_ONLY.evidence-index.json `
  --profile vehicle-data/profiles/EXAMPLE_ONLY.vehicle-profile.json `
  --observation vehicle-data/observations/EXAMPLE_ONLY.oem-start-observation.json `
  --checklist vehicle-data/profiles/EXAMPLE_ONLY.remote-start-prerequisites.json
```

Les validateurs Phase 3D acceptent en plus `--signal-sources` et
`--synchronized-start` pour contrôler la qualification par usage et les
bitfields des timelines synchronisées.

Les preuves acceptées sont : capture ISTA, export TestO, sortie INPA, sortie
Tool32, future capture CAN et note manuelle. La catégorie « future capture CAN »
est une provenance réservée ; cette phase n'acquiert et ne décode aucune donnée
BMW.

## Confiance et maturité

Une donnée utilise `UNKNOWN`, `LOW`, `MEDIUM`, `HIGH` ou `VALIDATED`.
`UNKNOWN` impose une valeur absente et aucune preuve. Tout autre niveau impose
une valeur, une date et une preuve existante.

La checklist emploie une maturité distincte : `UNKNOWN`, `OBSERVED`,
`CONFIRMED`, `VALIDATED` ou `BLOCKED`. Une observation isolée ne devient donc
pas automatiquement une donnée validée.

## Imports

L'importeur ne dépend d'aucun logiciel propriétaire et ne connaît aucun nom de
colonne ISTA ou TestO. Un mapping JSON associe les colonnes d'un CSV/TSV/texte
délimité au format canonique. Les sorties texte libres et relevés manuels
doivent d'abord être placés dans un tableau délimité ; leur preuve brute reste
archivée séparément.

```powershell
python tools/import_vehicle_data.py `
  --input vehicle-data/imports/EXAMPLE_ONLY.input.csv `
  --mapping vehicle-data/imports/EXAMPLE_ONLY.mapping.json `
  --evidence vehicle-data/evidence/EXAMPLE_ONLY.evidence-index.json `
  --output build/EXAMPLE_ONLY.imported-observation.json
```

Cette couche ne remplace pas une revue humaine : elle normalise, vérifie la
forme, la provenance et les incohérences élémentaires seulement.
