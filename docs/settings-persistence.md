# Persistance redondante des réglages

## Objectif

Le boîtier doit conserver une configuration après coupure d'alimentation sans
transformer une écriture interrompue ou une mémoire corrompue en réglage valide.
`JournaledUserSettingsStore` fournit cette logique sans dépendre de l'ESP32, de
NVS, d'une EEPROM ou d'un système de fichiers particulier.

## Organisation

Le stockage utilise deux emplacements fixes de 64 octets, soit une capacité
minimale de 128 octets. Chaque sauvegarde écrit l'emplacement qui ne contient pas
la génération valide la plus récente. L'ancienne génération reste donc disponible
jusqu'à la validation complète de la nouvelle.

| Offset | Taille | Contenu |
|---:|---:|---|
| 0 | 4 | Signature ASCII `BMRC` |
| 4 | 2 | Version de schéma, actuellement `3` |
| 6 | 2 | Taille de la charge utile |
| 8 | 4 | Compteur de génération |
| 12 | 40 | Valeurs `UserSettings` encodées explicitement |
| 52 | 4 | CRC-32 de l'en-tête et de la charge utile |

L'enregistrement V3 occupe 56 octets dans chaque emplacement. Les entiers sont
encodés explicitement en little-endian ; la disposition mémoire du compilateur
n'est jamais enregistrée directement.

Les octets 24 à 31 du payload contiennent le masque de fonctionnalités
modulaires. Les octets 32 à 39 contiennent les quatre seuils de télémétrie V1.
Le lecteur reconnaît aussi les enregistrements V1 de 40 octets et V2 de 48
octets. Il initialise les champs absents avec leurs valeurs par défaut ; la
prochaine sauvegarde produit une V3.

## Chargement

Au démarrage, les deux emplacements sont lus indépendamment. Un enregistrement
n'est accepté que si :

- sa signature, sa version et sa taille sont reconnues ;
- son CRC-32 est exact ;
- les énumérations et le booléen ont une représentation valide ;
- `validateUserSettings()` accepte toutes les valeurs et leur cohérence.

Si les deux emplacements sont valides, la génération la plus récente est choisie.
Si la plus récente est corrompue, la précédente est utilisée. Si aucune n'est
valide, `loadUserSettingsFailSafe()` fournit un profil sûr avec
`remoteStartEnabled=false`.

## Enregistrement

Une configuration invalide est rejetée avant toute écriture. Pour une valeur
valide, le journal :

1. lit les deux générations ;
2. choisit l'emplacement ancien ou vide ;
3. encode la génération suivante et son CRC ;
4. écrit puis demande `commit()` ;
5. relit l'enregistrement et compare chaque réglage.

Un échec d'écriture, de commit ou de vérification retourne `false`. L'adaptateur
matériel doit garantir qu'une écriture non validée par `commit()` ne remplace pas
atomiquement la dernière donnée durable.

## Port matériel

`SettingsByteStorage` expose seulement `capacity()`, `read()`, `write()` et
`commit()`. L'adaptateur ESP32 futur pourra s'appuyer sur NVS ou une partition
dédiée, mais devra préserver les deux emplacements, les erreurs de retour et les
sémantiques de commit.

Le scénario `settings-recovery` du simulateur enregistre deux configurations,
corrompt volontairement la plus récente et démontre le retour automatique à la
précédente.
