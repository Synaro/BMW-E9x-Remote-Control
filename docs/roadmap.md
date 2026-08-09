# Feuille de route post-audit

Cette feuille de route remplace les anciens jalons qui mélangeaient code livré,
idées et matériel futur. Les phases sont séquentielles : aucune phase ne doit
commencer sans validation explicite de la précédente.

## Phase 1 — Simplification post-audit

Objectif : réduire la dette et établir une description fidèle de l'existant,
sans ajouter de comportement.

- [x] Limiter le catalogue aux trois fonctions réellement implémentées
- [x] Déplacer les 40 idées restantes dans `docs/backlog/features.md`
- [x] Supprimer les migrations de configuration pré-V1 inutiles
- [x] Centraliser le calcul CRC32
- [x] Utiliser un manifeste unique pour les sources CMake et PowerShell
- [x] Commencer la séparation de `tests/test_main.cpp` sans réécrire les tests
- [x] Ajouter les builds PlatformIO natif et ESP32-S3 à la CI
- [x] Aligner la documentation sur l'état réellement livré
- [x] Retirer MCP2515 de la cible matérielle officielle
- [x] Valider tous les tests et builds locaux, puis la CI GitHub

## Phase 2 — Socle ESP32-S3 sûr

- [x] Adopter ESP-IDF 5.5.0 comme environnement embarqué de référence
- [x] Versionner `sdkconfig.defaults` et la table de partitions
- [x] Remplacer l'adaptateur Arduino/EEPROM par NVS
- [x] Définir la HAL minimale GPIO, temps, stockage et sûreté du futur TWAI
- [x] Activer watchdogs et buffers USB bornés sans tâche applicative superflue
- [x] Documenter silence matériel, inhibition TX et validation sur banc
- [x] Prouver au build l'absence d'installation et d'émission TWAI

## Phase 3 — TWAI en écoute seule

- [x] Définir le port de réception CAN indépendant d'Espressif et de BMW
- [x] Implémenter l'adaptateur ESP32-S3/TWAI en mode écoute seule
- [x] Garantir par construction et en CI l'absence d'émission
- [x] Ajouter file fixe, compteurs, erreurs et horodatage bornés
- [x] Définir le format Capture V2 et son manifeste de session

## Phase 3B — Matériel CAN de banc

- [x] Comparer les transceivers automobiles à partir des datasheets constructeur
- [x] Retenir le TCAN1057AV-Q1 et une inhibition TX physique indépendante
- [x] Définir le schéma de principe, les pulls sûrs et les GPIO BENCH_ONLY
- [x] Définir la procédure électrique et la checklist PASS/FAIL
- [ ] Assembler et inspecter le banc physique
- [ ] Exécuter et archiver tous les essais électriques
- [ ] Obtenir un PASS complet avant toute demande de connexion véhicule

## Phase 3C — Préparation des données véhicule

- [x] Définir un profil versionné pour l'identification réelle du véhicule
- [x] Définir une session ordonnée d'observation du démarrage OEM
- [x] Indexer la provenance ISTA, TestO, INPA, Tool32, capture CAN ou note manuelle
- [x] Ajouter un import tabulaire générique sans dépendance propriétaire
- [x] Créer la checklist versionnée des prérequis remote-start
- [x] Valider schémas, champs obligatoires, confiance, provenance et cohérence
- [ ] Intégrer les relevés réels après réception et revue des données utilisateur

La Phase 3C reste strictement documentaire et hors véhicule. Elle ne décode
aucun signal BMW et n'ajoute ni émission CAN, ni commande CAS/DDE. Son contrat
est détaillé dans
[phase3c-vehicle-data-intake.md](phase3c-vehicle-data-intake.md).

## Phase 3D — Qualification des preuves réelles

- [x] Qualifier les sources signal par signal et par usage
- [x] Archiver une timeline synchronisée RPM/MSA sans inférer la fonction des bits
- [x] Conserver les bitfields en décimal, hexadécimal, binaire et bits modifiés
- [x] Interdire la promotion automatique d'une corrélation vers une sémantique fonctionnelle
- [x] Confirmer l'état KL50 en lecture sans inférer son mécanisme d'activation
- [x] Corroborer la transition KL50 par un trace IFH Level 1
- [ ] Mesurer la durée KL50 et l'alignement RPM avec un trace IFH Level 3
- [ ] Valider l'algorithme de détection moteur tournant sur plusieurs captures structurées
- [ ] Identifier Terminal 50, la demande START et l'autorisation OEM par preuves séparées

- [x] Intégrer les identifications réelles CAS, DDE et EGS avec provenance
- [x] Qualifier KL15, P/R/N/D, frein et régime moteur par source
- [x] Marquer le mapping ISTA Transmission position comme `UNTRUSTED`
- [x] Interdire structurellement les données `UNTRUSTED` ou `BLOCKED` comme préconditions candidates
- [x] Ajouter le modèle observationnel `EngineRunStateObservation`
- [x] Préparer l'import générique d'un CSV de régime TestO
- [x] Archiver l'ordre réel des RPM sans inventer de timestamps
- [ ] Importer le CSV TestO brut et produire sa timeline réellement horodatée
- [ ] Valider les seuils candidats sur plusieurs démarrages OEM indépendants

Les résultats et limites de cette phase sont détaillés dans
[phase3d-real-evidence-qualification.md](phase3d-real-evidence-qualification.md).

## Phase 4 — Capture et analyse hors ligne

- [ ] Implémenter le transport et l'écriture PC du format Capture V2
- [ ] Valider et stabiliser le format avec des captures de banc
- [ ] Renforcer l'analyse différentielle et les rapports reproductibles
- [ ] Ajouter les jeux de validation sans données personnelles

## Phase 5 — Base de données des trames

- [ ] Définir un schéma versionné pour identifiants, signaux et preuves
- [ ] Enregistrer véhicule, variante, provenance et date de validation
- [ ] Gérer niveaux de confiance et contradictions
- [ ] Valider le schéma avec des captures de plusieurs véhicules

## Phase 6 — Bibliothèque BMW E8x/E9x

- [ ] Créer une bibliothèque indépendante du contrôleur distant
- [ ] Décoder uniquement les trames suffisamment validées
- [ ] Documenter unités, cadence, compteurs et contrôles d'intégrité
- [ ] Tester la compatibilité inter-véhicules et les cas inconnus

## Phase 7 — API véhicule en lecture seule

- [ ] Exposer des signaux sémantiques qualifiés au domaine
- [ ] Relier fraîcheur, plausibilité et profils véhicule
- [ ] Remplacer progressivement les sources synthétiques dans les essais de banc
- [ ] Conserver toutes les commandes véhicule désactivées

## Phase 8 — Commande sur banc et sûreté matérielle

- [ ] Réaliser l'analyse de risques avant toute sortie physique
- [ ] Concevoir alimentation, protections, watchdog et interverrouillages
- [ ] Implémenter les actionneurs uniquement sur charges factices
- [ ] Mener les campagnes HIL et d'injection de défauts

## Phase 9 — Interfaces utilisateur

- [ ] Stabiliser une API authentifiée et anti-rejeu
- [ ] Concevoir l'application iOS
- [ ] Ajouter un client Android sur la même API
- [ ] Maintenir chaque option désactivable dans les limites de sûreté

## Phase 10 — Qualification véhicule contrôlée

- [ ] Revue indépendante du logiciel, du schéma et des risques
- [ ] Qualification par variante E8x/E9x
- [ ] Essais progressifs avec faisceau réversible
- [ ] Documentation d'installation et procédure de retour à l'origine

Les idées fonctionnelles non planifiées restent dans
[backlog/features.md](backlog/features.md) et n'entrent dans aucune phase sans
revue explicite.
