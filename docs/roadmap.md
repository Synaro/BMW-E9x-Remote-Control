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
- [ ] Valider tous les tests et builds locaux, puis la CI GitHub

## Phase 2 — Couche CAN en écoute seule

- [ ] Définir le port de réception CAN indépendant d'Espressif
- [ ] Implémenter l'adaptateur ESP32-S3/TWAI en mode écoute seule
- [ ] Garantir par construction l'absence d'émission
- [ ] Ajouter compteurs, erreurs et horodatage bornés
- [ ] Valider sur bus de banc avant toute connexion véhicule

## Phase 3 — Capture et analyse hors ligne

- [ ] Stabiliser le format de capture canonique
- [ ] Relier les captures TWAI au format canonique
- [ ] Renforcer l'analyse différentielle et les rapports reproductibles
- [ ] Ajouter les jeux de validation sans données personnelles

## Phase 4 — Base de données des trames

- [ ] Définir un schéma versionné pour identifiants, signaux et preuves
- [ ] Enregistrer véhicule, variante, provenance et date de validation
- [ ] Gérer niveaux de confiance et contradictions
- [ ] Valider le schéma avec des captures de plusieurs véhicules

## Phase 5 — Bibliothèque BMW E8x/E9x

- [ ] Créer une bibliothèque indépendante du contrôleur distant
- [ ] Décoder uniquement les trames suffisamment validées
- [ ] Documenter unités, cadence, compteurs et contrôles d'intégrité
- [ ] Tester la compatibilité inter-véhicules et les cas inconnus

## Phase 6 — API véhicule en lecture seule

- [ ] Exposer des signaux sémantiques qualifiés au domaine
- [ ] Relier fraîcheur, plausibilité et profils véhicule
- [ ] Remplacer progressivement les sources synthétiques dans les essais de banc
- [ ] Conserver toutes les commandes véhicule désactivées

## Phase 7 — Commande sur banc et sûreté matérielle

- [ ] Réaliser l'analyse de risques avant toute sortie physique
- [ ] Concevoir alimentation, protections, watchdog et interverrouillages
- [ ] Implémenter les actionneurs uniquement sur charges factices
- [ ] Mener les campagnes HIL et d'injection de défauts

## Phase 8 — Interfaces utilisateur

- [ ] Stabiliser une API authentifiée et anti-rejeu
- [ ] Concevoir l'application iOS
- [ ] Ajouter un client Android sur la même API
- [ ] Maintenir chaque option désactivable dans les limites de sûreté

## Phase 9 — Qualification véhicule contrôlée

- [ ] Revue indépendante du logiciel, du schéma et des risques
- [ ] Qualification par variante E8x/E9x
- [ ] Essais progressifs avec faisceau réversible
- [ ] Documentation d'installation et procédure de retour à l'origine

Les idées fonctionnelles non planifiées restent dans
[backlog/features.md](backlog/features.md) et n'entrent dans aucune phase sans
revue explicite.
