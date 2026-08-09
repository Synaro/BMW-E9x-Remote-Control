# Phase 3D — Intégration et qualification des preuves réelles

## Périmètre

La Phase 3D qualifie les informations ISTA, Tool32 et TestO fournies pour la BMW
E90 de test. Elle reste entièrement hors véhicule et observationnelle. Aucun
fichier firmware, identifiant CAN BMW, décodeur BMW, chemin de transmission,
Terminal 50 ou mécanisme de démarrage n'est ajouté.

Les preuves disponibles sont des transcriptions manuelles anonymisées. Elles
ne sont pas présentées comme les exports bruts des outils. La première séquence
RPM reste ordonnée sans timestamps inventés. La seconde observation fournit des
timestamps synchronisés RPM/MSA consignés tels qu'ils ont été relevés.

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

La qualification d'une source est propre au signal et à l'usage. Une même
lecture peut ainsi être confirmée pour KL15 tout en étant interdite pour
déterminer si le moteur tourne. Les états `NOT_SUITABLE_FOR_PN`,
`NOT_FUNCTIONALLY_IDENTIFIED` et `OBSERVED_NO_TRANSITION` empêchent toute
promotion implicite vers une précondition.

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

### KL50

Le Job-Info de `CAS.PRG/status_fzg_zustand` documente `KLEMMENSTATUS` comme
quatre champs de deux bits : Klemme R en bits 0-1, Klemme 15 en bits 2-3,
Klemme 50 en bits 4-5 et validité clé en bits 6-7. Pour chaque terminal,
`00 = OFF`, `01 = ON` et `11 = invalide`.

Un démarrage OEM réel a produit la séquence ordonnée `69 / 0x45`,
`85 / 0x55`, puis `69 / 0x45`. Les bits KL15 restent à `01`, tandis que les
bits KL50 suivent `00 -> 01 -> 00`. L'état KL50 exposé en lecture est donc
`CONFIRMED` pour cette observation.

Cette transition est désormais corroborée par la transcription d'un trace IFH
EDIABAS réel de niveau 1. Le trace contient 64/`0x40`, 65/`0x41`, plusieurs
lectures consécutives à 85/`0x55`, puis un retour à 69/`0x45`. Elle n'est donc
pas traitée comme un artefact visuel de Tool32.

Le fichier brut `ifh.trc` n'est pas versionné et les timestamps individuels ne
sont pas disponibles. La durée KL50 reste `PENDING_LEVEL3_TRACE` et
l'alignement KL50/RPM reste `PENDING`. Aucune durée n'est reconstruite à partir
de l'ordre des lignes du trace Level 1.

Cette qualification est `READ_ONLY_SIGNAL` et reste `PROHIBITED` comme
précondition. Elle ne prouve ni la source de demande OEM, ni une commande de
démarrage, ni une autorisation, ni un ID CAN ou une commande diagnostique.

### Transmission et frein

`GS19D.PRG/status_getriebeposition` suit correctement les positions physiques
P/R/N/D et est `CONFIRMED`. Le job retourne `JOB_STATUS = OKAY`; une lecture en
P a notamment donné la valeur brute décimale 8 et le texte P.

Le champ ISTA `Transmission position` est `UNTRUSTED` sur cette configuration :
les positions physiques P/R/N/D ont produit R/N/D/D. Il est obligatoirement
`PROHIBITED` comme précondition.

ISTA `Actual gear` est `OBSERVED`, mais renvoie notamment `1st gear` en P, N et
D. Il ne peut pas distinguer P/N et reste `PROHIBITED` pour cet usage.

ISTA `Brake actuated` suit correctement pédale relâchée/appuyée sur plusieurs
actions physiques. La source reste `OBSERVED / COHERENT` jusqu'à archivage
d'une seconde source ou d'une capture structurée et n'est donc pas candidate.

### Sources CAS non temps réel

Les champs `BREMSE_AKTIV` et `WAHLHEBEL_NICHT_IN_P_AKTIV` de
`CAS.PRG/status_kl15_abschaltung` sont restés à 0 pendant les changements
physiques essayés. Ils sont `NOT_SUITABLE_AS_LIVE_SIGNAL` : ils décrivent une
logique/configuration de coupure KL15 observée, pas un état instantané prouvé du
frein ou du sélecteur.

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

### Corrélation RPM / STATUS_MSA

Une seconde observation OEM synchronisée conserve les changements de
`MSAAV`, `MSAAA` et `MSAEV` avec leur valeur décimale, hexadécimale, binaire et
les bits modifiés. Le premier changement MSA relevé à 9,365 s précède le
premier régime non nul à 9,427 s de 62 ms. Les bits candidats observés sont le
bit 5 pour `MSAAV` et le bit 2 pour `MSAAA`/`MSAEV`.

Il s'agit exclusivement de « bits MSA corrélés temporellement à un démarrage
OEM », avec le statut `OBSERVED / NOT FUNCTIONALLY IDENTIFIED`. Le schéma et le
validateur interdisent de transformer cette corrélation en KL50, demande de
démarreur, autorisation de démarrage ou demande CAS. `MSAEA` est resté constant
à 11 et porte le statut `OBSERVED_NO_TRANSITION`.

## CSV TestO

`tools/import_engine_speed_log.py` accepte un CSV délimité via un mapping
générique. Il normalise le timestamp en microsecondes relatives, conserve la
ligne source et produit les transitions observées.

Le mapping du véhicule réel est un gabarit : les noms de colonnes, le séparateur
et l'unité temporelle doivent être remplacés après inspection d'un export CSV.
La première timeline versionnée conserve seulement l'ordre et les RPM fournis,
avec `timestamp_basis: UNAVAILABLE`; la timeline RPM/MSA distincte conserve les
timestamps synchronisés explicitement fournis.

## Inconnues et blocages actuels

- exports TestO bruts : `BLOCKED` par absence des fichiers source dans le dépôt ;
- algorithme de détection moteur tournant : `NOT_YET_VALIDATED` ;
- signification fonctionnelle des bits MSA observés : `UNKNOWN` ;
- état Terminal 50 : `CONFIRMED` en lecture via KLEMMENSTATUS ;
- transition OEM KL50 OFF/ON/OFF : `CONFIRMED` par Tool32 et trace IFH ;
- durée KL50 : `PENDING_LEVEL3_TRACE` ;
- alignement temporel KL50/RPM : `PENDING` ;
- source de demande de démarrage OEM : `UNKNOWN` ;
- bouton START / demande de démarrage CAS : `UNKNOWN` ;
- mécanisme d'actionnement remote-start : `UNKNOWN` ;
- autorisation OEM de démarrage : `UNKNOWN` ;
- stratégie d'arrêt : `UNKNOWN` ;
- stratégie de timeout définitive : `UNKNOWN` ;
- perte de communication et comportement après reset : `UNKNOWN` ;
- politique complète des conditions empêchant le démarrage : `UNKNOWN` ;
- identifiants CAN BMW : `UNKNOWN`, aucun identifiant ajouté.

La Phase 4 et le remote-start réel restent non commencés.
