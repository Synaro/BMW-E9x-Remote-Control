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

Le fichier brut `ifh.trc` n'est pas versionné. Le trace Level 1 reste dépourvu
de timestamps individuels et ne porte donc aucune durée.

Une transcription distincte du trace IFH Level 3 conserve maintenant quatre
échantillons frontières : dernier OFF à `20:47:20.578` (`0x41`), premier ON à
`20:47:20.632` (`0x55`), dernier ON à `20:47:21.352` (`0x55`) et premier OFF à
`20:47:21.405` (`0x45`). Quinze échantillons ON consécutifs sont rapportés,
avec une période locale approximative de 46 à 58 ms, soit environ 20 Hz.

La durée est qualifiée `OBSERVED_BOUNDED / CONFIRMED_FROM_LEVEL3_TRACE` : le
minimum directement observé est 720 ms et la borne maximale liée aux
intervalles d'échantillonnage est 827 ms. La valeur 774 ms est uniquement le
milieu arrondi de ces bornes, avec une résolution approximative de 50 ms ; elle
n'est jamais stockée ou décrite comme une durée exacte. Cette preuve IFH seule
ne fournit pas de base temporelle commune avec le régime DDE ; elle ne permet
donc aucun alignement KL50/RPM.

### Corrélation diagnostique TestO 2.0 CAS / DDE

Une acquisition supplémentaire emploie un seul processus TestO 2.0 et une
seule session EDIABAS. Chaque boucle lit d'abord
`CAS.PRG/STATUS_FZG_ZUSTAND/KLEMMENSTATUS`, puis
`D71N47C0.PRG/STATUS_MOTORDREHZAHL/STAT_MOTORDREHZAHL_WERT`. Les timestamps
`Date.now()` appartiennent à la même horloge, mais les deux valeurs ne sont pas
échantillonnées simultanément.

Le texte fourni annonce 300 échantillons. L'analyse a récupéré 291 objets JSON
complets et monotones ; neuf lignes sont entremêlées dans la sortie textuelle.
Les échantillons critiques 9 à 13 sont intacts et donnent :

- sample 9 : CAS `0x45` / KL50 OFF à `1786303279519`, puis DDE 0 rpm à
  `1786303279572` ;
- sample 10 : CAS `0x55` / KL50 ON à `1786303279776`, puis DDE 131 rpm à
  `1786303279829` ;
- sample 11 : KL50 ON, puis 224,5 rpm ;
- sample 12 : KL50 ON à `1786303280347`, puis 863 rpm à `1786303280399` ;
- sample 13 : KL50 OFF à `1786303280653`, puis 981,5 rpm à
  `1786303280706`.

Les seules bornes soutenues par l'échantillonnage sont :

- passage KL50 OFF vers ON après `1786303279519` et au plus tard à
  `1786303279776`, fenêtre de 257 ms ;
- passage RPM 0 vers non nul après `1786303279572` et au plus tard à
  `1786303279829`, fenêtre de 257 ms ;
- passage KL50 ON vers OFF après `1786303280347` et au plus tard à
  `1786303280653`, fenêtre de 306 ms ;
- durée KL50 ON de cette acquisition bornée entre 571 et 1134 ms.

Ces bornes utilisent les débuts d'appels comme points observés. Une seconde
lecture, volontairement plus conservatrice, tient compte de la fin possible de
chaque appel : KL50 ON est alors borné dans une fenêtre de 310 ms, RPM non nul
dans 517 ms et KL50 OFF dans 359 ms. La durée KL50 compatible avec ces
intervalles d'appels est comprise entre 518 et 1187 ms. Aucune de ces valeurs
n'est un instant ou une durée physique exacte.

Les fenêtres KL50 ON et RPM non nul se recouvrent. Les écarts de 53, 52 et
53 ms observés dans les samples 10, 12 et 13 sont des écarts de lectures
diagnostiques CAS-puis-DDE, pas des délais physiques. La corrélation est donc
`CONFIRMED_DIAGNOSTIC_CORRELATION`, l'alignement devient
`OBSERVED_BOUNDED`, mais l'ordre physique reste `UNKNOWN`.

Sur les 272 échantillons récupérables de 19 à 299, le régime est compris entre
773 et 786,5 rpm, avec une médiane de 781 rpm et une moyenne de 780,557 rpm.
Cette statistique décrit la stabilisation observée et ne définit aucun seuil
final de détection moteur tournant.

Cette qualification est `READ_ONLY_SIGNAL` et reste `PROHIBITED` comme
précondition. Elle ne prouve ni la source de demande OEM, ni une commande de
démarrage, ni une autorisation, ni un ID CAN ou une commande diagnostique.

### Deuxième acquisition : cycle OEM complet

La session `OEM_FULL_CYCLE_01` contient 300 enregistrements JSON complets. Le
sample 0 consigne un échec initial des deux lectures ; les samples 1 à 299 sont
valides. La suite CAS réellement présente est `64 -> 65 -> 85 -> 69 -> 64`.
Le décodage documenté donne :

| Décimal | Hex | KL R | KL15 | KL50 | Schl Valid |
|---:|---:|---|---|---|---|
| 64 | `0x40` | OFF | OFF | OFF | ON |
| 65 | `0x41` | ON | OFF | OFF | ON |
| 69 | `0x45` | ON | ON | OFF | ON |
| 85 | `0x55` | ON | ON | ON | ON |

Au démarrage, les samples 29 à 31 rapportent KL50 ON avec 130, 341,5 puis
1022,5 rpm. Le sample 32 rapporte KL50 OFF avec 962 rpm. Les bornes fondées sur
les débuts d'appels donnent 514 à 1079 ms pour la durée observée ; les bornes
conservatrices tenant compte des appels séquentiels donnent 461 à 1132 ms.
Ces deux intervalles restent des bornes diagnostiques.

À l'arrêt, les samples 102 à 106 donnent respectivement `(69, 778,5)`,
`(64, 779,5)`, `(64, 438,5)`, `(64, 215)` puis `(64, 0)` pour
`(KLEMMENSTATUS, rpm)`. Le retour `69 -> 64` est donc observé avant que la
lecture DDE atteigne zéro, puis le régime décroît. Cela confirme uniquement la
séquence OEM lue ; aucune « commande stop » n'est identifiée.

Le passage `64 -> 65` coïncide avec la séquence d'insertion de clé déclarée
par l'utilisateur. Sa qualification reste `OBSERVED_CORRELATION`, sans
causalité. L'appui frein, les appuis START/STOP, le codage autorisant le départ
sans frein et l'éjection automatique de clé sont conservés séparément comme
`USER_DECLARED_ACTION`. Les captures ne mesurent ni frein, ni bouton, ni
éjection de clé.

### Comparaison des deux démarrages

Les deux sessions ont chacune trois samples KL50 ON accompagnés d'un régime
croissant. Le retour KL50 OFF est lu à 981,5 rpm dans `OEM_START_SYNC_01` et à
962 rpm dans `OEM_FULL_CYCLE_01`. Les différences entre bornes de durée sont
de 57 ms pour les minima et 55 ms pour les maxima ; l'écart de régime au
retour OFF est de 19,5 rpm. Deux démarrages ne suffisent pas à définir un seuil
moteur tournant ou un algorithme de désengagement du démarreur.

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

## Observateur OEM read-only

Les exports statiques locaux `CAS.PRG` et `D71N47C0.PRG` alimentent désormais
un catalogue machine-readable limité aux résultats de statut utiles. Les jobs
d'écriture, de contrôle, d'authentification, de programmation et les données
EWS/ISN sont exclus du runtime. L'existence d'un résultat dans le SGBD vaut
`DOCUMENTED_NOT_OBSERVED`, jamais validation sur le véhicule.

Le domaine définit `StartSignalSnapshot` avec un intervalle début/fin propre à
chaque sous-acquisition. Les quatre jobs CAS/DDE restent donc séquentiels et ne
sont jamais réduits à un timestamp unique. `VehicleStartObserver`, dans la
couche application, produit seulement des états suffixés `Observed` et les
preuves signal/source/intervalle associées. Il n'expose aucune action.

Les sessions `OEM_START_SYNC_01` et `OEM_FULL_CYCLE_01` qualifient uniquement
les reconnaissances déjà observées : `0x55` avec RPM non nul, retour
`0x55 -> 0x45` avec rotation persistante, stabilisation observée sur plusieurs
lectures, puis `0x45 -> 0x40` et décroissance jusqu'à 0. Aucun seuil RPM
universel n'est introduit.

## Inconnues et blocages actuels

- fichiers TestO bruts : analysés et hachés, mais volontairement conservés hors Git ;
- algorithme de détection moteur tournant : `NOT_YET_VALIDATED` ;
- signification fonctionnelle des bits MSA observés : `UNKNOWN` ;
- état Terminal 50 : `CONFIRMED` en lecture via KLEMMENSTATUS ;
- transition OEM KL50 OFF/ON/OFF : `CONFIRMED` par Tool32 et trace IFH ;
- durée KL50 : `OBSERVED_BOUNDED / CONFIRMED_FROM_LEVEL3_TRACE`, entre 720 et 827 ms ;
- corrélation diagnostique KL50/RPM sur horloge commune : `OBSERVED_BOUNDED` ;
- ordre physique exact KL50/RPM : `UNKNOWN` en raison des lectures séquentielles ;
- source de demande de démarrage OEM : `UNKNOWN` ;
- bouton START / demande de démarrage CAS : `UNKNOWN` ;
- mécanisme d'actionnement remote-start : `UNKNOWN` ;
- autorisation OEM de démarrage : `UNKNOWN` ;
- séquence d'arrêt OEM : `CONFIRMED_AS_SEQUENCE`, mécanisme d'arrêt : `UNKNOWN` ;
- stratégie de timeout définitive : `UNKNOWN` ;
- perte de communication et comportement après reset : `UNKNOWN` ;
- politique complète des conditions empêchant le démarrage : `UNKNOWN` ;
- identifiants CAN BMW : `UNKNOWN`, aucun identifiant ajouté.

La Phase 4 et le remote-start réel restent non commencés.
