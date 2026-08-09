# Phase 3D — Intégration et qualification des preuves réelles

## Périmètre

La Phase 3D qualifie les informations ISTA, Tool32 et TestO fournies pour la BMW
E90 de test. Elle reste entièrement hors véhicule et observationnelle. Aucun
fichier firmware, identifiant CAN BMW, décodeur BMW, chemin de transmission,
Terminal 50 ou mécanisme de démarrage n'est ajouté.

Les preuves disponibles sont des transcriptions manuelles anonymisées. Elles
ne sont pas présentées comme les exports bruts des outils. Le CSV TestO réel
n'étant pas encore dans le dépôt, aucun timestamp de cette capture n'est
inventé.

## Qualification

Chaque observation réelle utilise l'un des niveaux suivants :

- `OBSERVED` : comportement vu et cohérent, mais pas encore suffisamment répété ;
- `CONFIRMED` : observation répétée ou corroborée par une deuxième lecture ;
- `UNTRUSTED` : donnée observée mais incorrecte ou ambiguë pour l'usage visé ;
- `BLOCKED` : qualification impossible tant qu'une preuve précise manque ;
- `UNKNOWN` : aucune observation exploitable.

Une observation contient directement l'outil, le SGBD/PRG, le job, le champ, le
contexte physique, la date de consignation, la session, la preuve, les valeurs
brute et interprétée, l'unité et les notes.

`precondition_eligibility` ne possède que deux valeurs : `PROHIBITED` et
`CANDIDATE`. Il n'existe aucun état actif. Le validateur impose
`PROHIBITED` à toute donnée `UNKNOWN`, `OBSERVED`, `UNTRUSTED` ou `BLOCKED`.
Seule une donnée `CONFIRMED` explicitement marquée `CANDIDATE` passe la garde
hôte, sans pour autant devenir une précondition du contrôleur.

## Résultats intégrés

### Identification

- véhicule : BMW E90 berline, N47, boîte automatique, date ISTA 2008/09 ;
- CAS : `CAS.PRG`, K-CAN, pièce `9147226`, indices et versions consignés ;
- EGS : `GS19D.PRG`, PT-CAN, pièce `7591971`, indices et versions consignés ;
- DDE : `D71N47C0.PRG`, PT-CAN, pièce `7823420`, versions et dates consignées.

Ces identifications sont `CONFIRMED`. Les numéros de pièce ne sont jamais
convertis implicitement en numéros ZB.

### KL15

`KLEMMENSTATUS` via `CAS.PRG/status_fzg_zustand` est confirmé : 64 pour le
véhicule réveillé contact coupé, 65 pour la clé insérée avant KL15, 69 pour KL15
actif. La valeur 69 existe moteur arrêté et moteur tournant : elle est donc
explicitement exclue comme preuve autonome de moteur tournant.

### Transmission et frein

`GS19D.PRG/status_getriebeposition` suit correctement les positions physiques
P/R/N/D et est `CONFIRMED`.

Le champ ISTA `Transmission position` est `UNTRUSTED` sur cette configuration :
les positions physiques P/R/N/D ont produit R/N/D/D. Il est obligatoirement
`PROHIBITED` comme précondition.

ISTA `Actual gear` est `OBSERVED`, mais renvoie notamment `1st gear` en P, N et
D. Il ne peut pas distinguer P/N et reste `PROHIBITED` pour cet usage.

ISTA `Brake actuated` suit correctement pédale relâchée/appuyée. La source reste
`OBSERVED` jusqu'à répétition suffisante et n'est donc pas encore candidate.

### Régime et état moteur

Tool32 confirme 0 rpm moteur arrêté et environ 780 rpm au ralenti. TestO
confirme un ralenti autour de 786,5 rpm. La séquence ordonnée réelle contient
les transitoires 890 et 1022,5 rpm avant la stabilisation : une règle naïve
`RPM > 700` est ainsi exclue.

`EngineRunStateObservation` reste un modèle hôte purement observationnel avec
`STOPPED`, `CRANKING`, `RUNNING` et `UNKNOWN`. L'importeur CSV exige une
configuration externe marquée `candidate_only`, une bande d'entrée, une
hystérésis de sortie et plusieurs échantillons consécutifs. Aucun seuil n'est
compilé dans le firmware.

## CSV TestO

`tools/import_engine_speed_log.py` accepte un CSV délimité via un mapping
générique. Il normalise le timestamp en microsecondes relatives, conserve la
ligne source et produit les transitions observées.

Le mapping du véhicule réel est un gabarit : les noms de colonnes, le séparateur
et l'unité temporelle doivent être remplacés après inspection du vrai CSV. La
timeline actuellement versionnée conserve seulement l'ordre et les RPM fournis,
avec `timestamp_basis: UNAVAILABLE`.

## Inconnues et blocages actuels

- CSV TestO brut et timestamps réels : `BLOCKED` par absence du fichier ;
- algorithme de détection moteur tournant : `NOT_YET_VALIDATED` ;
- Terminal 50 / demande de démarrage : `UNKNOWN` ;
- autorisation OEM de démarrage : `UNKNOWN` ;
- stratégie d'arrêt : `UNKNOWN` ;
- stratégie de timeout définitive : `UNKNOWN` ;
- perte de communication et comportement après reset : `UNKNOWN` ;
- politique complète des conditions empêchant le démarrage : `UNKNOWN` ;
- identifiants CAN BMW : `UNKNOWN`, aucun identifiant ajouté.

La Phase 4 et le remote-start réel restent non commencés.
